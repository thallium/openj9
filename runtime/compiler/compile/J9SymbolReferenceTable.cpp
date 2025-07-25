/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "omrformatconsts.h"
#include "codegen/CodeGenerator.hpp"
#include "env/KnownObjectTable.hpp"
#include "compile/AliasBuilder.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "cs2/hashtab.h"
#include "env/CHTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/PersistentInfo.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/VMJ9.h"
#include "env/j9method.h"
#include "env/jittypes.h"
#include "il/StaticSymbol.hpp"
#include "il/StaticSymbol_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "ilgen/IlGen.hpp"
#include "ilgen/J9ByteCodeIlGenerator.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/List.hpp"
#include "infra/String.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "env/PersistentCHTable.hpp"
#include "optimizer/TransformUtil.hpp"
#if defined(J9VM_OPT_JITSERVER)
#include "env/j9methodServer.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */

namespace J9
{
enum NonUserMethod
   {
   unknownNonUserMethod,
   nonUser_java_util_HashMap_rehash,
   nonUser_java_util_HashMap_analyzeMap,
   nonUser_java_util_HashMap_calculateCapacity,
   nonUser_java_util_HashMap_findNullKeyEntry,

   numNonUserMethods
   };
}


J9::SymbolReferenceTable::SymbolReferenceTable(size_t sizeHint, TR::Compilation *c) :
   OMR::SymbolReferenceTableConnector(sizeHint, c),
     _immutableInfo(c->trMemory()),
     _immutableSymRefNumbers(c->trMemory(), _numImmutableClasses),
     _dynamicMethodSymrefsByCallSiteIndex(c->trMemory()),
     _currentThreadDebugEventDataSymbol(0),
     _currentThreadDebugEventDataSymbolRefs(c->trMemory()),
     _constantPoolAddressSymbolRefs(c->trMemory()),
     _defaultValueAddressSlotSymbolRefs(c->trMemory()),
     _resolvedFieldShadows(
        std::less<ResolvedFieldShadowKey>(),
        getTypedAllocator<ResolvedFieldShadowsEntry>(c->allocator())),
    _flattenedArrayElementFieldShadows(
        std::less<ResolvedFieldShadowKey>(),
        getTypedAllocator<FlattenedArrayElementFieldShadowsEntry>(c->allocator()))
   {
   for (uint32_t i = 0; i < _numImmutableClasses; i++)
      _immutableSymRefNumbers[i] = new (trHeapMemory()) TR_BitVector(sizeHint, c->trMemory(), heapAlloc, growable);

   for (int i = 0; i < TR::Symbol::numberOfMemoryOrderings; i++)
      _unsafeJavaStaticSymRefs[i] = NULL;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateCountForRecompileSymbolRef()
   {
   if (!element(countForRecompileSymbol))
      {
      TR::StaticSymbol * sym = TR::StaticSymbol::create(trHeapMemory(),TR::Int32);
      TR::PersistentInfo *pinfo = comp()->getPersistentInfo();
      sym->setStaticAddress(&(pinfo->_countForRecompile));
      sym->setCountForRecompile();
      sym->setNotDataAddress();
      element(countForRecompileSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), countForRecompileSymbol, sym);
      }
   return element(countForRecompileSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateOSRBufferSymbolRef()
   {
   if (!element(osrBufferSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "OSRBuffer");
      sym->setDataType(TR::Address);
      sym->setNotCollected();
      element(osrBufferSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), osrBufferSymbol, sym);
      element(osrBufferSymbol)->setOffset(fej9->thisThreadGetOSRBufferOffset());

      // We can't let the load/store of the exception symbol swing down
      aliasBuilder.addressStaticSymRefs().set(getNonhelperIndex(osrBufferSymbol)); // add the symRef to the statics list to get correct aliasing info
      }
   return element(osrBufferSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateOSRScratchBufferSymbolRef()
   {
   if (!element(osrScratchBufferSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "OSRScratchBuffer");
      sym->setDataType(TR::Address);
      sym->setNotCollected();
      element(osrScratchBufferSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), osrScratchBufferSymbol, sym);
      element(osrScratchBufferSymbol)->setOffset(fej9->thisThreadGetOSRScratchBufferOffset());

      // We can't let the load/store of the exception symbol swing down
      aliasBuilder.addressStaticSymRefs().set(getNonhelperIndex(osrScratchBufferSymbol)); // add the symRef to the statics list to get correct aliasing info
      }
   return element(osrScratchBufferSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateOSRFrameIndexSymbolRef()
   {
   if (!element(osrFrameIndexSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "osrFrameIndex");
      sym->setDataType(TR::Int32);
      element(osrFrameIndexSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), osrFrameIndexSymbol, sym);
      element(osrFrameIndexSymbol)->setOffset(fej9->thisThreadGetOSRFrameIndexOffset());

      // We can't let the load/store of the exception symbol swing down
      //SD: do we need this?
      aliasBuilder.addressStaticSymRefs().set(getNonhelperIndex(osrFrameIndexSymbol)); // add the symRef to the statics list to get correct aliasing info
      }
   return element(osrFrameIndexSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateAcquireVMAccessSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_acquireVMAccess, true, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateThrowCurrentExceptionSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_throwCurrentException, true, false, true);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateThrowUnreportedExceptionSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_throwUnreportedException, true /* canGCandReturn */, false /* canGCandExcept */, true /* preservesAllRegisters */);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateReleaseVMAccessSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_releaseVMAccess, true, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateStackOverflowSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_stackOverflow, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateWriteBarrierStoreSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_writeBarrierStore, false, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateWriteBarrierStoreGenerationalSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_writeBarrierStoreGenerational, false, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateWriteBarrierStoreGenerationalAndConcurrentMarkSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_writeBarrierStoreGenerationalAndConcurrentMark, false, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateWriteBarrierStoreRealTimeGCSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_writeBarrierStoreRealTimeGC, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateWriteBarrierClassStoreRealTimeGCSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_writeBarrierClassStoreRealTimeGC, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateWriteBarrierBatchStoreSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_writeBarrierBatchStore, false, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateAcmpeqHelperSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol)
   {
   return findOrCreateRuntimeHelper(TR_acmpeqHelper, true, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateAcmpneHelperSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol)
   {
   return findOrCreateRuntimeHelper(TR_acmpneHelper, true, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateGetFlattenableFieldSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol)
   {
   return findOrCreateRuntimeHelper(TR_getFlattenableField, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateWithFlattenableFieldSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol)
   {
   return findOrCreateRuntimeHelper(TR_withFlattenableField, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreatePutFlattenableFieldSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol)
   {
   return findOrCreateRuntimeHelper(TR_putFlattenableField, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateGetFlattenableStaticFieldSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol)
   {
   return findOrCreateRuntimeHelper(TR_getFlattenableStaticField, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreatePutFlattenableStaticFieldSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol)
   {
   return findOrCreateRuntimeHelper(TR_putFlattenableStaticField, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateLoadFlattenableArrayElementSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol)
   {
   return findOrCreateRuntimeHelper(TR_ldFlattenableArrayElement, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateStoreFlattenableArrayElementSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol)
   {
   return findOrCreateRuntimeHelper(TR_strFlattenableArrayElement, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateLookupDynamicPublicInterfaceMethodSymbolRef()
   {
   return findOrCreateRuntimeHelper(TR_jitLookupDynamicPublicInterfaceMethod, false, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateDispatchJ9MethodSymbolRef()
   {
   TR_ASSERT_FATAL(comp()->cg()->enableJitDispatchJ9Method(), "not enabled");

   TR::SymbolReference *symRef = element(jitDispatchJ9MethodSymbol);
   if (symRef == NULL)
      {
      TR::MethodSymbol *sym = TR::MethodSymbol::create(trHeapMemory(), TR_None);
      sym->setHelper();
      sym->setIsInlinedByCG();
      sym->setLinkage(TR_Private);
      TR::SymbolReference *symRef = new (trHeapMemory()) TR::SymbolReference(
         self(), jitDispatchJ9MethodSymbol, sym);

      symRef->setCanGCandReturn();
      symRef->setCanGCandExcept();
      element(jitDispatchJ9MethodSymbol) = symRef;
      }

   return element(jitDispatchJ9MethodSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateFloatSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   void * dataAddress = owningMethodSymbol->getResolvedMethod()->floatConstant(cpIndex);
   TR::SymbolReference * symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Float, true, dataAddress);
   symRef->getSymbol()->setConst();
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateDoubleSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   void * dataAddress = owningMethodSymbol->getResolvedMethod()->doubleConstant(cpIndex, trMemory());
   TR::SymbolReference * symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Double, true, dataAddress);
   symRef->getSymbol()->setConst();
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::createSystemRuntimeHelper(
      TR_RuntimeHelper index,
      bool canGCandReturn,
      bool canGCandExcept,
      bool preservesAllRegisters)
   {
   TR::SymbolReference * symRef = createRuntimeHelper(index,false,false,false);
   if (symRef)
      {
      symRef->getSymbol()->castToMethodSymbol()->setSystemLinkageDispatch();
      symRef->getSymbol()->castToMethodSymbol()->setLinkage(TR_System);
      }
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateConstantPoolAddressSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   void *cpAddress = owningMethodSymbol->getResolvedMethod()->constantPool();
   ListIterator<TR::SymbolReference> i(&_constantPoolAddressSymbolRefs);
   TR::SymbolReference * symRef;
   for (symRef = i.getFirst(); symRef; symRef = i.getNext())
      if (symRef->getSymbol()->getStaticSymbol()->getStaticAddress() == cpAddress)
         return symRef;

   TR::StaticSymbol * sym = TR::StaticSymbol::create(trHeapMemory(),TR::Address);
   sym->setStaticAddress(cpAddress);
   sym->setConstantPoolAddress();
   sym->setNotCollected();
   sym->setNotDataAddress();

   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), -1);
   _constantPoolAddressSymbolRefs.add(symRef);
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateStaticMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   bool isUnresolvedInCP;
   TR_ResolvedMethod * method = owningMethodSymbol->getResolvedMethod()->getResolvedStaticMethod(comp(), cpIndex, &isUnresolvedInCP);
   if (method)
      owningMethodSymbol->setMayHaveInlineableCall(true);

   return findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), cpIndex, method, TR::MethodSymbol::Static, isUnresolvedInCP);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateSpecialMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   bool isUnresolvedInCP;
   TR_ResolvedMethod * method = owningMethodSymbol->getResolvedMethod()->getResolvedSpecialMethod(comp(), cpIndex, &isUnresolvedInCP);
   if (method)
      owningMethodSymbol->setMayHaveInlineableCall(true);

   return findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), cpIndex, method, TR::MethodSymbol::Special, isUnresolvedInCP);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateVirtualMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   bool isUnresolvedInCP;
   TR_ResolvedMethod * method = owningMethodSymbol->getResolvedMethod()->getResolvedVirtualMethod(comp(), cpIndex, false, &isUnresolvedInCP);
   if (method)
      owningMethodSymbol->setMayHaveInlineableCall(true);

   return findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), cpIndex, method, TR::MethodSymbol::Virtual, false); //isUnresolvedInCP);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateInterfaceMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   owningMethodSymbol->setMayHaveInlineableCall(true);
   return findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), cpIndex, 0, TR::MethodSymbol::Interface);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateDynamicMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t callSiteIndex, bool * unresolvedInCP, bool * isInvokeCacheAppendixNull)
   {
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
   TR_ResolvedMethod  * method = owningMethodSymbol->getResolvedMethod()->getResolvedDynamicMethod(comp(), callSiteIndex, unresolvedInCP, isInvokeCacheAppendixNull);
   if (method)
      owningMethodSymbol->setMayHaveInlineableCall(true);

   TR::SymbolReference * symRef = findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), -1, method, TR::MethodSymbol::Static);
#else
   List<TR::SymbolReference> *methods = dynamicMethodSymrefsByCallSiteIndex(callSiteIndex);
   ListIterator<TR::SymbolReference> li(methods);
   for (TR::SymbolReference *symRef = li.getFirst(); symRef; symRef = li.getNext())
      {
      if (symRef->getOwningMethodIndex() == owningMethodSymbol->getResolvedMethodIndex())
         return symRef;
      }
   TR_ResolvedMethod  * method = owningMethodSymbol->getResolvedMethod()->getResolvedDynamicMethod(comp(), callSiteIndex, unresolvedInCP);
   TR::SymbolReference * symRef = findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), -1, method, TR::MethodSymbol::ComputedVirtual);
   methods->add(symRef);
#endif /* J9VM_OPT_OPENJDK_METHODHANDLE */
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateHandleMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex, char *signature)
   {
   TR_ResolvedMethod  * method = owningMethodSymbol->getResolvedMethod()->getResolvedHandleMethodWithSignature(comp(), cpIndex, signature);
   if (method)
      owningMethodSymbol->setMayHaveInlineableCall(true);

   TR::SymbolReference * symRef = findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), cpIndex, method, TR::MethodSymbol::ComputedVirtual);
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateHandleMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex, bool * unresolvedInCP, bool * isInvokeCacheAppendixNull)
   {
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
   TR_ResolvedMethod  * method = owningMethodSymbol->getResolvedMethod()->getResolvedHandleMethod(comp(), cpIndex, unresolvedInCP, isInvokeCacheAppendixNull);
   TR::SymbolReference * symRef = findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), cpIndex, method, TR::MethodSymbol::Static);
   if (*unresolvedInCP)
      symRef->getSymbol()->setDummyResolvedMethod(); // linkToStatic is a dummy TR_ResolvedMethod
#else
   TR_ResolvedMethod  * method = owningMethodSymbol->getResolvedMethod()->getResolvedHandleMethod(comp(), cpIndex, unresolvedInCP);
   TR::SymbolReference * symRef = findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), cpIndex, method, TR::MethodSymbol::ComputedVirtual);
#endif /* J9VM_OPT_OPENJDK_METHODHANDLE */
   if (method)
      owningMethodSymbol->setMayHaveInlineableCall(true);
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateCallSiteTableEntrySymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t callSiteIndex)
   {
   TR::SymbolReference *symRef;
   TR_SymRefIterator i(aliasBuilder.callSiteTableEntrySymRefs(), self());
   TR_ResolvedMethod *owningMethod = owningMethodSymbol->getResolvedMethod();
   void *entryLocation = owningMethod->callSiteTableEntryAddress(callSiteIndex);
   for (symRef = i.getNext(); symRef; symRef = i.getNext())
      if (  owningMethodSymbol->getResolvedMethodIndex() == symRef->getOwningMethodIndex()
         && symRef->getSymbol()->castToStaticSymbol()->getStaticAddress() == entryLocation)
         {
         return symRef;
         }

   TR::StaticSymbol *sym = TR::StaticSymbol::create(trHeapMemory(),TR::Address);
   sym->makeCallSiteTableEntry(callSiteIndex);
   sym->setStaticAddress(entryLocation);
   bool isUnresolved = owningMethod->isUnresolvedCallSiteTableEntry(callSiteIndex);

   TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN;
   if (!isUnresolved)
      {
      TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
      if (knot)
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
         knownObjectIndex = knot->getOrCreateIndexAt((uintptr_t*)entryLocation, true);
#else
         knownObjectIndex = knot->getOrCreateIndexAt((uintptr_t*)entryLocation);
#endif
      }

   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), -1,
                                                    (isUnresolved ? _numUnresolvedSymbols++ : 0), knownObjectIndex);

   if (isUnresolved)
      {
      // Resolving call site table entries causes java code to run
      symRef->setUnresolved();
      symRef->setCanGCandReturn();
      symRef->setCanGCandExcept();
      }

   aliasBuilder.callSiteTableEntrySymRefs().set(symRef->getReferenceNumber());
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateMethodTypeTableEntrySymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   TR::SymbolReference *symRef;
   TR_SymRefIterator i(aliasBuilder.methodTypeTableEntrySymRefs(), self());
   TR_ResolvedMethod *owningMethod = owningMethodSymbol->getResolvedMethod();
   void *entryLocation = owningMethod->methodTypeTableEntryAddress(cpIndex);
   for (symRef = i.getNext(); symRef; symRef = i.getNext())
      if (  owningMethodSymbol->getResolvedMethodIndex() == symRef->getOwningMethodIndex()
         && symRef->getSymbol()->castToStaticSymbol()->getStaticAddress() == entryLocation)
         {
         return symRef;
         }

   TR::StaticSymbol *sym = TR::StaticSymbol::createMethodTypeTableEntry(trHeapMemory(),cpIndex);
   sym->setStaticAddress(entryLocation);
   bool isUnresolved = owningMethod->isUnresolvedMethodTypeTableEntry(cpIndex);

   TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN;
   if (!isUnresolved)
      {
      TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
      if (knot)
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
         knownObjectIndex = knot->getOrCreateIndexAt((uintptr_t*)entryLocation, true);
#else
         knownObjectIndex = knot->getOrCreateIndexAt((uintptr_t*)entryLocation);
#endif
      }

   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), -1,
                                                    (isUnresolved ? _numUnresolvedSymbols++ : 0), knownObjectIndex);

   if (isUnresolved)
      {
      // Resolving method type table entries causes java code to run
      symRef->setUnresolved();
      symRef->setCanGCandReturn();
      symRef->setCanGCandExcept();
      }

   aliasBuilder.methodTypeTableEntrySymRefs().set(symRef->getReferenceNumber());
   return symRef;
   }

#if defined(J9VM_OPT_METHOD_HANDLE)
TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateVarHandleMethodTypeTableEntrySymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   // This function does the same thing as what findOrCreateMethodTypeTableEntrySymbol does except the way it gets the method type table entry address
   TR::SymbolReference *symRef;
   TR_SymRefIterator i(aliasBuilder.methodTypeTableEntrySymRefs(), self());
   TR_ResolvedJ9Method *owningMethod = (TR_ResolvedJ9Method*)(owningMethodSymbol->getResolvedMethod());
   void *entryLocation = owningMethod->varHandleMethodTypeTableEntryAddress(cpIndex);
   for (symRef = i.getNext(); symRef; symRef = i.getNext())
      if (  owningMethodSymbol->getResolvedMethodIndex() == symRef->getOwningMethodIndex()
         && symRef->getSymbol()->castToStaticSymbol()->getStaticAddress() == entryLocation)
         {
         return symRef;
         }

   TR::StaticSymbol *sym = TR::StaticSymbol::createMethodTypeTableEntry(trHeapMemory(),cpIndex);
   sym->setStaticAddress(entryLocation);
   bool isUnresolved = owningMethod->isUnresolvedVarHandleMethodTypeTableEntry(cpIndex);

   TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN;
   if (!isUnresolved)
      {
      TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
      if (knot != NULL)
         {
         knownObjectIndex = knot->getOrCreateIndexAt((uintptr_t*)entryLocation);
         }
      }

   symRef = new (trHeapMemory()) TR::SymbolReference(
      self(),
      sym,
      owningMethodSymbol->getResolvedMethodIndex(),
      -1,
      isUnresolved ? _numUnresolvedSymbols++ : 0,
      knownObjectIndex);

   if (isUnresolved)
      {
      // Resolving method type table entries causes java code to run
      symRef->setUnresolved();
      symRef->setCanGCandReturn();
      symRef->setCanGCandExcept();
      }

   aliasBuilder.methodTypeTableEntrySymRefs().set(symRef->getReferenceNumber());
   return symRef;
   }
#endif /* defined(J9VM_OPT_METHOD_HANDLE) */


TR::SymbolReference *
J9::SymbolReferenceTable::methodSymRefWithSignature(TR::SymbolReference *originalSymRef, char *effectiveSignature, int32_t effectiveSignatureLength)
   {
   TR::ResolvedMethodSymbol *originalSymbol = originalSymRef->getSymbol()->castToResolvedMethodSymbol();
   TR_ASSERT(originalSymbol, "methodSymRefWithSignature requires a resolved method symref");
   TR_ASSERT(!originalSymbol->isVirtual(), "methodSymRefFromName doesn't support virtual methods"); // Until we're able to look up vtable index
   int32_t cpIndex = originalSymRef->getCPIndex();

   // Check _methodsBySignature to see if we've already created a symref for this one
   //
   TR::Method *originalMethod = originalSymbol->getMethod();

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   int32_t fullSignatureLength = originalMethod->classNameLength() + 1 + originalMethod->nameLength() + effectiveSignatureLength;
   char *fullSignature = (char*)trMemory()->allocateMemory(1 + fullSignatureLength, stackAlloc);
   snprintf(fullSignature, 1 + fullSignatureLength, "%.*s.%.*s%.*s", originalMethod->classNameLength(), originalMethod->classNameChars(), originalMethod->nameLength(), originalMethod->nameChars(), effectiveSignatureLength, effectiveSignature);
   TR_ASSERT(strlen(fullSignature) == fullSignatureLength, "Computed fullSignatureLength must match actual length of fullSignature");
   CS2::HashIndex hashIndex = 0;
   static char *ignoreMBSCache = feGetEnv("TR_ignoreMBSCache");
   OwningMethodAndString key(originalSymRef->getOwningMethodIndex(), fullSignature);
   if (_methodsBySignature.Locate(key, hashIndex) && !ignoreMBSCache)
      {
      TR::SymbolReference *result = _methodsBySignature[hashIndex];
      if (comp()->getOption(TR_TraceMethodIndex))
         traceMsg(comp(), "-- MBS cache hit (2): M%p\n", result->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod());
      return result;
      }
   else
      {
      // fullSignature will be kept as a key by _methodsBySignature, so it needs heapAlloc
      //
      key = OwningMethodAndString(originalSymRef->getOwningMethodIndex(), self()->strdup(fullSignature));
      if (comp()->getOption(TR_TraceMethodIndex))
         traceMsg(comp(), "-- MBS cache miss (2) owning method #%d, signature %s\n", originalSymRef->getOwningMethodIndex().value(), fullSignature);
      }

   //
   // No existing symref.  Create a new one.
   //

   TR_OpaqueMethodBlock *method = originalSymbol->getResolvedMethod()->getPersistentIdentifier();

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   // Note: we use cpIndex=-1 here so we don't end up getting back originalSymRef (which will not have the signature we want)
   TR::SymbolReference *result = findOrCreateMethodSymbol(originalSymRef->getOwningMethodIndex(), -1,
      fej9->createResolvedMethodWithSignature(comp()->trMemory(), method, NULL, effectiveSignature, effectiveSignatureLength, originalSymbol->getResolvedMethod()->owningMethod()), originalSymbol->getMethodKind());

   result->setCPIndex(cpIndex);
   _methodsBySignature.Add(key, result);
   TR_ASSERT(_methodsBySignature.Locate(key), "After _methodsBySignature.Add, _methodsBySignature.Locate must be true");

   return result;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateTypeCheckArrayStoreSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_typeCheckArrayStore, false, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateArrayClassRomPtrSymbolRef()
   {
   if (!element(arrayClassRomPtrSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Address);
      element(arrayClassRomPtrSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), arrayClassRomPtrSymbol, sym);
      element(arrayClassRomPtrSymbol)->setOffset(fej9->getOffsetOfArrayClassRomPtrField());
      if (!TR::Compiler->cls.romClassObjectsMayBeCollected())
         sym->setNotCollected();
      }
   return element(arrayClassRomPtrSymbol);
   }


// Right now it only works for private fields or fields that are guaranteed to be accessed only from one class
// because searchRecognizedField only does a name comparison, t does not work if the field is accessed from a subclass of the expected one
TR::SymbolReference *
J9::SymbolReferenceTable::findOrFabricateShadowSymbol(TR::ResolvedMethodSymbol *owningMethodSymbol,
                                                      TR::Symbol::RecognizedField recognizedField,
                                                      TR::DataType type,
                                                      uint32_t offset,
                                                      bool isVolatile,
                                                      bool isPrivate,
                                                      bool isFinal,
                                                      const char *name)
   {
   TR_ResolvedMethod * owningMethod = owningMethodSymbol->getResolvedMethod();

   //The following code (up to and including the call to initShadowSymbol) has been adapted from
   //J9::SymbolReferenceTable::findOrCreateShadowSymbol
   TR::SymbolReference * symRef = NULL;
   TR::Symbol * sym = NULL;

   if (!comp()->compileRelocatableCode()
#if defined(J9VM_OPT_JITSERVER)
       && !comp()->isOutOfProcessCompilation()
#endif
       )
      {
      symRef = findShadowSymbol(owningMethod, -1, type, &recognizedField);

      if (symRef)
         return symRef;
      }
   sym = createShadowSymbol(
      type,
      isVolatile,
      isPrivate,
      isFinal,
      name,
      recognizedField);

   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), -1);
   // isResolved = true, isUnresolvedInCP = false
   initShadowSymbol(owningMethod, symRef, true, type, offset, false);
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findResolvedFieldShadow(
   ResolvedFieldShadowKey key,
   bool isVolatile,
   bool isPrivate,
   bool isFinal)
   {
   const auto entry = _resolvedFieldShadows.find(key);
   if (entry == _resolvedFieldShadows.end())
      return NULL;

   TR::SymbolReference *symRef = entry->second;
   int32_t refNum = symRef->getReferenceNumber();
   TR::Symbol *sym = symRef->getSymbol();

   // The following asserts enforce restrictions on certain symbols properties.
   //
   // Taking volatility as an example, it is possible to find a symref in _resolvedFieldShadows
   // whose symbols is marked isVolatile but that is for a non-volatile field. Consider a scenario
   // where, during compilation, an unresolved field reference is encountered. We will create a
   // symbol and conservatively mark is volatile. Later, a resolved reference for the same field is
   // encountered (note that it need not have the same cpIndex as the first field ref). Being
   // resolved, we determine that it is non-volatile. Because a symref for the field
   // already exists but has a different resolution state, a new *symref* is created but the found
   // *symbol* is reused. Since volatility is a property of the symbol, the new symref will share
   // the volatility of the first symref (which was unresolved and must therefore be conservatively
   // considered volatile). The new symref is then added to _resolvedFieldShadows. The next time
   // the same field reference is encountered, the symref for it is found in _resolvedFieldShadows
   // and once again its symbol is marked as volatile despite the field being non-volatile.
   //
   // The inverse condition, however, is not currently possible (i.e. finding a non-volatile symbol
   // for a volatile field). Still, it's possible that could change in the future (for accesses
   // with different memory ordering effects). If so, it should be made part of the key, and the
   // two symRefs differing only in isVolatile should be considered to possibly alias.
   //
   // Similar restrictions apply to the isPrivate and isFinal symbol properties, although the truth
   // conditions are reversed.
   TR_ASSERT_FATAL(sym->isVolatile() || !isVolatile, "expecting volatile symref but found non-volatile symref #%d\n", refNum);
   TR_ASSERT_FATAL(!sym->isPrivate() || isPrivate, "expecting non-private symref but found private symref #%d\n", refNum);
   TR_ASSERT_FATAL(!sym->isFinal() || isFinal, "expecting non-final symref but found final symref #%d\n", refNum);

   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrFabricateShadowSymbol(
   TR_OpaqueClassBlock *containingClass,
   TR::DataType type,
   uint32_t offset,
   bool isVolatile,
   bool isPrivate,
   bool isFinal,
   const char *name,
   const char *signature)
   {
   ResolvedFieldShadowKey key(containingClass, offset, type);
   TR::SymbolReference *symRef = findResolvedFieldShadow(key, isVolatile, isPrivate, isFinal);
   if (symRef != NULL)
      return symRef;

   int32_t classNameLen = 0;
   const char *className =
      TR::Compiler->cls.classNameChars(comp(), containingClass, classNameLen);

   int qualifiedFieldNameSize = classNameLen + 1 + strlen(name) + 1 + strlen(signature) + 1;
   char *qualifiedFieldName = (char*)trHeapMemory().allocate(qualifiedFieldNameSize);
   TR::snprintfNoTrunc(
      qualifiedFieldName,
      qualifiedFieldNameSize,
      "%.*s.%s %s",
      classNameLen,
      className,
      name,
      signature);

   TR::Symbol *sym = createShadowSymbol(
      type,
      isVolatile,
      isPrivate,
      isFinal,
      qualifiedFieldName,
      TR::Symbol::UnknownField);

   TR_J9VM *fej9 = reinterpret_cast<TR_J9VM *>(fe());
   TR_OpaqueClassBlock *declaredClass = fej9->getClassFromSignature(signature, (int32_t)strlen(signature), containingClass);
   if (declaredClass != NULL)
      sym->setDeclaredClass(declaredClass);

   mcount_t methodIndex = mcount_t::valueOf(0);
   int32_t cpIndex = -1;
   symRef = new (trHeapMemory()) TR::SymbolReference(
      self(),
      sym,
      methodIndex,
      cpIndex);

   bool isResolved = true;
   bool isUnresolvedInCP = false;
   initShadowSymbol(NULL, symRef, isResolved, type, offset, isUnresolvedInCP);

   _resolvedFieldShadows.insert(std::make_pair(key, symRef));
   return symRef;
   }

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
TR::SymbolReference *
J9::SymbolReferenceTable::findOrFabricateMemberNameVmTargetShadow()
   {
   bool isVolatile = false;
   bool isPrivate = false;
   bool isFinal = true;
   TR::SymbolReference *symRef = self()->findOrFabricateShadowSymbol(
      comp()->getMethodSymbol(),
      TR::Symbol::Java_lang_invoke_MemberName_vmtarget,
      TR::Address,
      comp()->fej9()->getVMTargetOffset(),
      isVolatile,
      isPrivate,
      isFinal,
      "java/lang/invoke/MemberName.vmtarget J");

   symRef->getSymbol()->setNotCollected();
   return symRef;
   }
#endif

TR::SymbolReference *
J9::SymbolReferenceTable::findFlattenedArrayElementFieldShadow(
   ResolvedFieldShadowKey key,
   bool isPrivate)
   {
   const auto entry = _flattenedArrayElementFieldShadows.find(key);
   if (entry == _flattenedArrayElementFieldShadows.end())
      return NULL;

   TR::SymbolReference *symRef = entry->second;
   int32_t refNum = symRef->getReferenceNumber();
   TR::Symbol *sym = symRef->getSymbol();

   TR_ASSERT_FATAL(sym->isPrivate() == isPrivate,  "expecting %s symref but found %s: symref #%d\n",
         isPrivate ? "private" : "non-private", sym->isPrivate() ? "private" : "non-private", refNum);

   // When an array element is flattened, a write through this field shadow doesn't represent a write
   // to the field. It is a partial write to an array element which is non volatile.
   TR_ASSERT_FATAL(sym->isVolatile() == false, "expecting non-volatile symref but found volatile: symref #%d\n", refNum);
   // The field is flattened into the array element which is mutable.
   TR_ASSERT_FATAL(sym->isFinal() == false, "expecting non-final symref but found final: symref #%d\n", refNum);

   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrFabricateFlattenedArrayElementFieldShadowSymbol(
   TR_OpaqueClassBlock *arrayComponentClass,
   TR::DataType type,
   int32_t fieldOffset,
   bool isPrivate,
   const char *fieldName,
   const char *fieldSignature)
   {
   TR_ASSERT_FATAL(fieldOffset >= 0, "fieldOffset %d is invalid: fieldOffset %u objectHeaderSizeInBytes %" OMR_PRIuPTR " \n", fieldOffset, fieldOffset, TR::Compiler->om.objectHeaderSizeInBytes());

   ResolvedFieldShadowKey key(arrayComponentClass, fieldOffset, type);

   TR::SymbolReference *symRef = findFlattenedArrayElementFieldShadow(key, isPrivate);
   if (symRef != NULL)
      return symRef;

   int32_t classNameLen = 0;
   const char *className = TR::Compiler->cls.classNameChars(comp(), arrayComponentClass, classNameLen);

   // "<Q-className-array-shadow>.fieldName fieldSignature"
   char qNameMem[128];
   TR::StringBuf qNameBuf(trMemory()->currentStackRegion(), qNameMem, sizeof(qNameMem));
   qNameBuf.appendf("<Q-%.*s-array-shadow>.%s %s", classNameLen, className, fieldName, fieldSignature);

   size_t qualifiedFieldNameSize = qNameBuf.len() + 1; // +1 for NULL terminator
   char *qualifiedFieldName = (char*)trHeapMemory().allocate(qualifiedFieldNameSize);
   memcpy(qualifiedFieldName, qNameBuf.text(), qualifiedFieldNameSize);

   // isVolatile is false because when an array element is flattened, a write through
   // this field shadow doesn't represent a write to the field. It is a partial write
   // to an array element which is non volatile.
   // isFinal is false because the field is flattened into the array element which is mutable.
   TR::Symbol *sym = createShadowSymbol(
      type,
      false /*isVolatile*/,
      isPrivate,
      false /*isFinal*/,
      qualifiedFieldName,
      TR::Symbol::UnknownField);

   mcount_t methodIndex = mcount_t::valueOf(0);
   int32_t cpIndex = -1;
   symRef = new (trHeapMemory()) TR::SymbolReference(
      self(),
      sym,
      methodIndex,
      cpIndex);

   bool isResolved = true;
   bool isUnresolvedInCP = false;
   initShadowSymbol(NULL, symRef, isResolved, type, fieldOffset, isUnresolvedInCP);

   _flattenedArrayElementFieldShadows.insert(std::make_pair(key, symRef));
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateDefaultValueSymbolRef(void *defaultValueSlotAddress, int32_t cpIndex)
   {
   ListIterator<TR::SymbolReference> i(&_defaultValueAddressSlotSymbolRefs);
   TR::SymbolReference *symRef;
   for (symRef = i.getFirst(); symRef; symRef = i.getNext())
      {
      if (symRef->getSymbol()->getStaticSymbol()->getStaticAddress() == defaultValueSlotAddress)
         {
         return symRef;
         }
      }

   TR::StaticSymbol *sym = TR::StaticSymbol::create(trHeapMemory(), TR::Address);

   sym->setStaticAddress(defaultValueSlotAddress);

   sym->setNotDataAddress();
   sym->setStaticDefaultValueInstance();

   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym);

   symRef->setCPIndex(cpIndex);
   symRef->setOwningMethodIndex(comp()->getMethodSymbol()->getResolvedMethodIndex());

   // TODO: Is this required?
   aliasBuilder.addressStaticSymRefs().set(symRef->getReferenceNumber());

   _defaultValueAddressSlotSymbolRefs.add(symRef);
   return symRef;
   }

TR::Symbol *
J9::SymbolReferenceTable::createShadowSymbol(
   TR::DataType type,
   bool isVolatile,
   bool isPrivate,
   bool isFinal,
   const char *name,
   TR::Symbol::RecognizedField recognizedField)
   {
   TR::Symbol *sym = TR::Symbol::createPossiblyRecognizedShadowWithFlags(
      trHeapMemory(), type, isVolatile, isFinal, isPrivate, recognizedField);

   if (name != NULL)
      {
      sym->setNamedShadowSymbol();
      sym->setName(name);
      }

   static char *dontAliasShadowsToEarlierGISEnv = feGetEnv("TR_dontAliasShadowsToEarlierGIS");
   bool dontAliasShadowsToEarlierGIS = dontAliasShadowsToEarlierGISEnv != NULL;

   if (aliasBuilder.mutableGenericIntShadowHasBeenCreated() && !dontAliasShadowsToEarlierGIS)
      {
      // Some previously-created GIS might actually refer to this shadow
      aliasBuilder.setConservativeGenericIntShadowAliasing(true);
      }

   return sym;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateShadowSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex, bool isStore)
   {
   TR_ResolvedJ9Method * owningMethod =
      static_cast<TR_ResolvedJ9Method*>(owningMethodSymbol->getResolvedMethod());

   bool isVolatile = true, isFinal = false, isPrivate = false, isUnresolvedInCP;
   TR::DataType type = TR::NoType;
   uint32_t offset = 0;
   bool resolved = owningMethod->fieldAttributes(comp(), cpIndex, &offset, &type, &isVolatile, &isFinal, &isPrivate, isStore, &isUnresolvedInCP, true);
   bool sharesSymbol = false;

   TR_OpaqueClassBlock *containingClass = NULL;
   TR::Symbol::RecognizedField recognizedField = TR::Symbol::searchRecognizedField(comp(), owningMethod, cpIndex, false);

   if (isStore && isPrivate && !comp()->getOptions()->realTimeGC() &&
       owningMethodSymbol->getResolvedMethod()->getRecognizedMethod() == TR::java_lang_ref_SoftReference_get &&
       recognizedField == TR::Symbol::Java_lang_ref_SoftReference_age)
      {
      isVolatile = false;
      }

   if (resolved)
      {
      bool isStatic = false;
      TR_OpaqueClassBlock* fromResolvedJ9Method = NULL;
      containingClass =
         owningMethod->definingClassFromCPFieldRef(comp(), cpIndex, isStatic, &fromResolvedJ9Method);

      // Normally, the expectation is that if a cp field ref is resolved, then the cp ref pointing
      // to the containing class must also be resolved. However, in #9416, it was discovered that
      // this assumption does not hold in some cases. Under AOT, loading class data from the SCC
      // using findSharedData() may fail, which is propagated to the JIT as a NULL containing class.
      // In addition, if a class is redefined between the point where a field is looked up and
      // where the defining class is looked, the latter may become NULL.
      //
      // Having a NULL containing class would imply two fields from different classes could have
      // the same lookup key, which is a problem. So, until the above issues are resolved, the
      // safe thing to do is to abort compilation in these (rare) cases.
      if (containingClass == NULL)
         comp()->failCompilation<TR::CompilationException>(
            "failed to get defining class of resolved field ref cpIndex=%d in owning method J9Method=%p",
            cpIndex,
            owningMethod->getNonPersistentIdentifier());

      ResolvedFieldShadowKey key(containingClass, offset, type);
      TR::SymbolReference *symRef =
         findResolvedFieldShadow(key, isVolatile, isPrivate, isFinal);

      if (symRef != NULL)
         return symRef;
      }

   TR::Symbol * sym = 0;

   TR::SymbolReference * symRef = findShadowSymbol(owningMethod, cpIndex, type, &recognizedField);
   if (symRef)
      {
      if ((resolved && !symRef->isUnresolved()) ||
          (!resolved && symRef->isUnresolved() && owningMethod == symRef->getOwningMethod(comp())))
         return symRef;
      sym = symRef->getSymbol()->castToShadowSymbol();
      sharesSymbol = true;
      }
   else
      {
      sym = createShadowSymbol(
         type,
         isVolatile,
         isPrivate,
         isFinal,
         NULL,
         recognizedField);
      }

   if (resolved)
      {
      int32_t len = 0;
      const char *sig = owningMethod->fieldSignatureChars(cpIndex, len);
      TR_OpaqueClassBlock *declaredClass =
         fe()->getClassFromSignature(sig, len, owningMethod);

      if (declaredClass != NULL)
         {
         TR_OpaqueClassBlock *prevDeclaredClass = sym->getDeclaredClass();
         if (prevDeclaredClass == NULL)
            {
            sym->setDeclaredClass(declaredClass);
            }
         else
            {
            TR_ASSERT_FATAL(
               prevDeclaredClass == declaredClass,
               "declared class mismatch: %p vs. prev %p",
               declaredClass,
               prevDeclaredClass);
            }
         }
      }

   int32_t unresolvedIndex = resolved ? 0 : _numUnresolvedSymbols++;

   if (sharesSymbol)
     symRef->setReallySharesSymbol();

   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), cpIndex, unresolvedIndex);
   checkUserField(symRef);

   if (sharesSymbol)
     symRef->setReallySharesSymbol();

   initShadowSymbol(owningMethod, symRef, resolved, type, offset, isUnresolvedInCP);

   if (cpIndex > 0)
      aliasBuilder.cpSymRefs().set(symRef->getReferenceNumber());

   if (containingClass != NULL)
      {
      ResolvedFieldShadowKey key(containingClass, offset, type);
      _resolvedFieldShadows.insert(std::make_pair(key, symRef));
      }

   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateObjectNewInstanceImplSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol)
   {
   if (!_ObjectNewInstanceImplSymRef)
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)fe();
      TR_ResolvedMethod * resolvedMethod = fej9->getObjectNewInstanceImplMethod(trMemory());
      TR::ResolvedMethodSymbol * sym = TR::ResolvedMethodSymbol::create(trHeapMemory(),resolvedMethod,comp());
      sym->setMethodKind(TR::MethodSymbol::Virtual);

      mcount_t owningMethodIndex = owningMethodSymbol->getResolvedMethodIndex();

      _ObjectNewInstanceImplSymRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodIndex, -1, 0);
      _ObjectNewInstanceImplSymRef->setCanGCandReturn();
      _ObjectNewInstanceImplSymRef->setCanGCandExcept();
      _ObjectNewInstanceImplSymRef->setOffset(fej9->getNewInstanceImplVirtualCallOffset());

      aliasBuilder.methodSymRefs().set(_ObjectNewInstanceImplSymRef->getReferenceNumber()); // cmvc defect 193493

      // This is a dummy resolved method - would never be called.  However, we set
      // the count to zero here to make sure that the optimizer does not think that
      // this method is cold and never being called
      //
      resolvedMethod->setInvocationCount(resolvedMethod->getInvocationCount(), 0);
      }
   return _ObjectNewInstanceImplSymRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateDLTBlockSymbolRef()
   {
   if (!element(dltBlockSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "DLTBlockMeta");
      sym->setDataType(TR::Address);
      sym->setNotCollected();
      element(dltBlockSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), dltBlockSymbol, sym);
      element(dltBlockSymbol)->setOffset(fej9->thisThreadGetDLTBlockOffset());

      // We can't let the load/store of the exception symbol swing down
      //
      aliasBuilder.addressStaticSymRefs().set(getNonhelperIndex(dltBlockSymbol)); // add the symRef to the statics list to get correct aliasing info
      aliasBuilder.gcSafePointSymRefNumbers().set(getNonhelperIndex(dltBlockSymbol));
      }
   return element(dltBlockSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findDLTBlockSymbolRef()
   {
   return element(dltBlockSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateMultiANewArraySymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_multiANewArray, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateInstanceShapeSymbolRef()
   {
   if (!element(instanceShapeSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym;
      if (self()->comp()->target().is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(instanceShapeSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), instanceShapeSymbol, sym);
      element(instanceShapeSymbol)->setOffset(fej9->getOffsetOfInstanceShapeFromClassField());
      }
   return element(instanceShapeSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateInstanceDescriptionSymbolRef()
   {
   if (!element(instanceDescriptionSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym;
      if (self()->comp()->target().is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(instanceDescriptionSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), instanceDescriptionSymbol, sym);
      element(instanceDescriptionSymbol)->setOffset(fej9->getOffsetOfInstanceDescriptionFromClassField());
      }
   return element(instanceDescriptionSymbol);
   }



TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateDescriptionWordFromPtrSymbolRef()
   {
   if (!element(descriptionWordFromPtrSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym;
      if (self()->comp()->target().is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(descriptionWordFromPtrSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), descriptionWordFromPtrSymbol, sym);
      element(descriptionWordFromPtrSymbol)->setOffset(fej9->getOffsetOfDescriptionWordFromPtrField());
      }
   return element(descriptionWordFromPtrSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateClassFlagsSymbolRef()
   {
   if (!element(isClassFlagsSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(isClassFlagsSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), isClassFlagsSymbol, sym);
      element(isClassFlagsSymbol)->setOffset(fej9->getOffsetOfClassFlags());
      }
   return element(isClassFlagsSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateClassDepthAndFlagsSymbolRef()
   {
   if (!element(isClassDepthAndFlagsSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(isClassDepthAndFlagsSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), isClassDepthAndFlagsSymbol, sym);
      element(isClassDepthAndFlagsSymbol)->setOffset(fej9->getOffsetOfClassDepthAndFlags());
      }
   return element(isClassDepthAndFlagsSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateArrayComponentTypeAsPrimitiveSymbolRef()
   {
   if (!element(componentClassAsPrimitiveSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym;
      if (self()->comp()->target().is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);

      element(componentClassAsPrimitiveSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), componentClassAsPrimitiveSymbol, sym);
      element(componentClassAsPrimitiveSymbol)->setOffset(fej9->getOffsetOfArrayComponentTypeField());
      if (!TR::Compiler->cls.classObjectsMayBeCollected())
         sym->setNotCollected();
      }
   return element(componentClassAsPrimitiveSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateMethodTypeCheckSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_methodTypeCheck, false, true, true);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateIncompatibleReceiverSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_incompatibleReceiver, false, true, true);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateIdentityExceptionSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_identityException, false, true, true);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateIncompatibleClassChangeErrorSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_IncompatibleClassChangeError, false, true, true);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateReportStaticMethodEnterSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_reportStaticMethodEnter, true, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateReportMethodExitSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_reportMethodExit, true, false, true);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateHeaderFlagsSymbolRef()
   {
   if (!element(headerFlagsSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::SymbolReference * symRef;
      // object header flags now occupy 4bytes on 64-bit
      symRef = new (trHeapMemory()) TR::SymbolReference(self(), headerFlagsSymbol, TR::Symbol::createShadow(trHeapMemory(),TR::Int32));
      symRef->setOffset(TR::Compiler->om.offsetOfHeaderFlags());
      element(headerFlagsSymbol) = symRef;
      aliasBuilder.intShadowSymRefs().set(symRef->getReferenceNumber());
      }

   return element(headerFlagsSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateDiscontiguousArraySizeSymbolRef()
   {
   if (!element(discontiguousArraySizeSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym;

      // Size field is 32-bits on ALL header shapes.
      //
      sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(discontiguousArraySizeSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), discontiguousArraySizeSymbol, sym);
      element(discontiguousArraySizeSymbol)->setOffset(fej9->getOffsetOfDiscontiguousArraySizeField());
      }
   return element(discontiguousArraySizeSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateClassLoaderSymbolRef(TR_ResolvedMethod * method)
   {
   ListIterator<TR::SymbolReference> i(&_classLoaderSymbolRefs);
   TR::SymbolReference * symRef;
   for (symRef = i.getFirst(); symRef; symRef = i.getNext())
      if (symRef->getOwningMethod(comp()) == method)
         return symRef;

   TR::StaticSymbol * sym = TR::StaticSymbol::create(trHeapMemory(),TR::Address);
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   sym->setStaticAddress(fej9->getLocationOfClassLoaderObjectPointer(method->classOfMethod()));
   mcount_t index = comp()->getOwningMethodSymbol(method)->getResolvedMethodIndex();
   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, index, -1);

   aliasBuilder.addressStaticSymRefs().set(symRef->getReferenceNumber()); // add the symRef to the statics list to get correct aliasing info

   _classLoaderSymbolRefs.add(symRef);

   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateCurrentThreadSymbolRef()
   {
   if (!element(currentThreadSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "CurrentThread");
      sym->setDataType(TR::Address);
      if (fej9->isJ9VMThreadCurrentThreadImmutable())
         {
         sym->setImmutableField();
         }
      element(currentThreadSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), currentThreadSymbol, sym);
      element(currentThreadSymbol)->setOffset(fej9->thisThreadGetCurrentThreadOffset());
      }
   return element(currentThreadSymbol);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateJ9MethodConstantPoolFieldSymbolRef(intptr_t offset)
   {
   if (!element(j9methodConstantPoolSymbol))
      {
      TR::Symbol * sym;
      if (self()->comp()->target().is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(),TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(),TR::Int32);

      element(j9methodConstantPoolSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), j9methodConstantPoolSymbol, sym);
      element(j9methodConstantPoolSymbol)->setOffset(offset);
      }
   TR::SymbolReference *result = element(j9methodConstantPoolSymbol);
   TR_ASSERT(result->getOffset() == offset, "findOrCreateJ9MethodConstantPoolFieldSymbolRef must use the right offset!  %d != %d", result->getOffset(), offset);
   return result;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateJ9MethodExtraFieldSymbolRef(intptr_t offset)
   {
   if (!element(j9methodExtraFieldSymbol))
      {
      TR::Symbol * sym;
      if (self()->comp()->target().is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(),TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(),TR::Int32);

      element(j9methodExtraFieldSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), j9methodExtraFieldSymbol, sym);
      element(j9methodExtraFieldSymbol)->setOffset(offset);
      }
   TR::SymbolReference *result = element(j9methodExtraFieldSymbol);
   TR_ASSERT(result->getOffset() == offset, "findOrCreateJ9MethodExtraFieldSymbolRef must use the right offset!  %d != %d", result->getOffset(), offset);
   return result;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateJ9JNIMethodIDvTableIndexFieldSymbol(intptr_t offset)
   {
   if (!element(J9JNIMethodIDvTableIndexFieldSymbol))
      {
      TR::Symbol * sym;
      if (self()->comp()->target().is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(),TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(),TR::Int32);

      element(J9JNIMethodIDvTableIndexFieldSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), J9JNIMethodIDvTableIndexFieldSymbol, sym);
      element(J9JNIMethodIDvTableIndexFieldSymbol)->setOffset(offset);
      }
   return element(J9JNIMethodIDvTableIndexFieldSymbol);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateStartPCLinkageInfoSymbolRef(intptr_t offset)
   {
   if (!element(startPCLinkageInfoSymbol))
      {
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(),TR::Int32);
      element(startPCLinkageInfoSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), startPCLinkageInfoSymbol, sym);
      element(startPCLinkageInfoSymbol)->setOffset(offset);
      }
   TR::SymbolReference *result = element(startPCLinkageInfoSymbol);
   TR_ASSERT(result->getOffset() == offset, "findOrCreateStartPCLinkageInfoSymbolRef must use the right offset!  %d != %d", result->getOffset(), offset);
   return result;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreatePerCodeCacheHelperSymbolRef(TR_CCPreLoadedCode helper, uintptr_t helperAddr)
   {
   CommonNonhelperSymbol index = (CommonNonhelperSymbol)(firstPerCodeCacheHelperSymbol + helper);
   if (!element(index))
      {
      TR::MethodSymbol * methodSymbol = TR::MethodSymbol::create(trHeapMemory(),TR_Private);
      methodSymbol->setHelper();
      // The address of the helper depends on the current codecache, which can change during compilation, so we don't have a single method address
      methodSymbol->setMethodAddress(NULL);
      element(index) = new (trHeapMemory()) TR::SymbolReference(self(), index, methodSymbol);
      }
   return element(index);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateANewArraySymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_aNewArray, true, true, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateStringSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   TR_ResolvedMethod * owningMethod = owningMethodSymbol->getResolvedMethod();
   void * stringConst = owningMethod->stringConstant(cpIndex);
   TR::SymbolReference * symRef;
   bool isString = true;
   if (owningMethod->isUnresolvedString(cpIndex))
      {
      symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Address, false, 0);
      symRef->setOffset((uintptr_t)stringConst);
      }
   else
      {
      TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN;
      if (!comp()->compileRelocatableCode())
         {
         TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
         if (knot)
            {
            knownObjectIndex = knot->getOrCreateIndexAt((uintptr_t*)stringConst);
            }
         }
      symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Address, true, stringConst, knownObjectIndex);
      }

   TR::StaticSymbol * sym = (TR::StaticSymbol *)symRef->getSymbol();

   // If the cp entry is patched, it's resolved during the patching
   // Unresolved cp entry will always contain a const string
   if (symRef->isUnresolved())
      {
      sym->setConstString();
      }
   // If symbol is created the first time, check if the const object is String
   else if (!sym->isConstString() &&
            !sym->isNonSpecificConstObject())
      {
      // getObjectClassAt will acquire/release VMAccess internally when needed
      TR_OpaqueClassBlock *clazz = comp()->fej9()->getObjectClassAt((uintptr_t)stringConst);
      if (comp()->fej9()->isString(clazz))
         {
         sym->setConstString();
         }
      else
         {
         if (comp()->compileRelocatableCode())
            comp()->failCompilation<J9::AOTHasPatchedCPConstant>("Patched Constant not supported in AOT.");

         sym->setNonSpecificConstObject();
         }
      }

   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateConstantDynamicSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex, char* symbolTypeSig, int32_t symbolTypeSigLength, bool isCondyPrimitive)
   {
   TR_ResolvedMethod * owningMethod = owningMethodSymbol->getResolvedMethod();
   void * dynamicConst = owningMethod->dynamicConstant(cpIndex, NULL);
   TR::SymbolReference * symRef;
   if (owningMethod->isUnresolvedConstantDynamic(cpIndex))
      {
      symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Address, false, 0);
      symRef->setOffset((uintptr_t)dynamicConst);
      }
   else
      {
      TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN;
      TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
      if (knot != NULL)
         {
         knownObjectIndex = knot->getOrCreateIndexAt((uintptr_t*)dynamicConst);
         }

      symRef = findOrCreateCPSymbol(
         owningMethodSymbol,
         cpIndex,
         TR::Address,
         true,
         dynamicConst,
         knownObjectIndex);
      }

   TR::StaticSymbol * sym = (TR::StaticSymbol *)symRef->getSymbol();
   sym->setConstantDynamic();
   sym->makeConstantDynamic(symbolTypeSig, symbolTypeSigLength, isCondyPrimitive);
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateMethodMonitorEntrySymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_methodMonitorEntry, true, false, true);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateMethodMonitorExitSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_methodMonitorExit, true /* canGCandReturn */, true /* canGCandExcept */, true /* preservesAllRegisters */);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateCompiledMethodSymbolRef()
   {
   if (!element(compiledMethodSymbol))
      {
      TR::StaticSymbol * sym = TR::StaticSymbol::create(trHeapMemory(),TR::Address);
      sym->setStaticAddress(comp()->getCurrentMethod()->getPersistentIdentifier());
      sym->setCompiledMethod();
      sym->setNotDataAddress();
      element(compiledMethodSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), compiledMethodSymbol, sym);
      }
   return element(compiledMethodSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateMethodTypeSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   TR_ResolvedMethod * owningMethod = owningMethodSymbol->getResolvedMethod();
   void * methodTypeConst = owningMethod->methodTypeConstant(cpIndex);
   TR::SymbolReference * symRef;
   if (owningMethod->isUnresolvedMethodType(cpIndex))
      {
      symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Address, false, 0);
      symRef->setOffset((uintptr_t)methodTypeConst);
      }
   else
      {
      TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN;
      TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
      if (knot != NULL)
         {
         knownObjectIndex = knot->getOrCreateIndexAt((uintptr_t*)methodTypeConst);
         }

      symRef = findOrCreateCPSymbol(
         owningMethodSymbol,
         cpIndex,
         TR::Address,
         true,
         methodTypeConst,
         knownObjectIndex);
      }
   TR::StaticSymbol * sym = (TR::StaticSymbol *)symRef->getSymbol();
   sym->setConstMethodType();
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateMethodHandleSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex)
   {
   TR_ResolvedMethod * owningMethod = owningMethodSymbol->getResolvedMethod();
   void * methodHandleConst = owningMethod->methodHandleConstant(cpIndex);
   TR::SymbolReference * symRef;
   if (owningMethod->isUnresolvedMethodHandle(cpIndex))
      {
      symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Address, false, 0);
      symRef->setOffset((uintptr_t)methodHandleConst);
      }
   else
      {
      TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN;
      TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
      if (knot)
         knownObjectIndex = knot->getOrCreateIndexAt((uintptr_t*)methodHandleConst);
      symRef = findOrCreateCPSymbol(owningMethodSymbol, cpIndex, TR::Address, true, methodHandleConst, knownObjectIndex);
      }
   TR::StaticSymbol * sym = (TR::StaticSymbol *)symRef->getSymbol();
   sym->setConstMethodHandle();
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateClassFromJavaLangClassAsPrimitiveSymbolRef()
   {
   if (!element(classFromJavaLangClassAsPrimitiveSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym;
      if (self()->comp()->target().is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);

      element(classFromJavaLangClassAsPrimitiveSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), classFromJavaLangClassAsPrimitiveSymbol, sym);
      element(classFromJavaLangClassAsPrimitiveSymbol)->setOffset(fej9->getOffsetOfClassFromJavaLangClassField());
      }
   return element(classFromJavaLangClassAsPrimitiveSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findClassFromJavaLangClassAsPrimitiveSymbolRef()
   {
   return element(classFromJavaLangClassAsPrimitiveSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::createShadowSymbolWithoutCpIndex(TR::ResolvedMethodSymbol * owningMethodSymbol, bool isResolved,
                                                          TR::DataType type, uint32_t offset, bool isUnresolvedInCP)
   {
   TR_ResolvedMethod * owningMethod = owningMethodSymbol->getResolvedMethod();

   TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(),type);
   TR::SymbolReference * symRef;

   int32_t unresolvedIndex = isResolved ? 0 : _numUnresolvedSymbols++;
   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), -1);
   initShadowSymbol(owningMethod, symRef, isResolved, type, offset, isUnresolvedInCP);
   return symRef;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findJavaLangReferenceReferentShadowSymbol(TR_ResolvedMethod * owningMethod, TR::DataType type, uint32_t offset)
   {
   TR::SymbolReference * symRef;
   TR_SymRefIterator i(type == TR::Address ? aliasBuilder.addressShadowSymRefs() :
                                            (type == TR::Int32 ? aliasBuilder.intShadowSymRefs() : aliasBuilder.nonIntPrimitiveShadowSymRefs()), self());
   while ((symRef = i.getNext()) != NULL)
      if (symRef->getSymbol()->getDataType() == type &&
          symRef->getOffset() == offset &&
          symRef->getOwningMethod(comp()) == owningMethod)
         return symRef;
   return 0;
   }


void
J9::SymbolReferenceTable::initShadowSymbol(TR_ResolvedMethod * owningMethod, TR::SymbolReference *symRef, bool isResolved,
                                          TR::DataType type, uint32_t offset, bool isUnresolvedInCP)
   {
   if (isResolved)
      {
      symRef->setOffset(offset);
      }
   else
      {
      symRef->setUnresolved();
      symRef->setCanGCandExcept();
      aliasBuilder.unresolvedShadowSymRefs().set(symRef->getReferenceNumber());
      }

   symRef->setHasBeenAccessedAtRuntime(isUnresolvedInCP? TR_no : TR_maybe);

   if (type == TR::Address)
      aliasBuilder.addressShadowSymRefs().set(symRef->getReferenceNumber());
   else if (type == TR::Int32)
      aliasBuilder.intShadowSymRefs().set(symRef->getReferenceNumber());
   else
      aliasBuilder.nonIntPrimitiveShadowSymRefs().set(symRef->getReferenceNumber());

   if (shouldMarkBlockAsCold(owningMethod, isUnresolvedInCP))
      markBlockAsCold();

   return;
   }


List<TR::SymbolReference> *
J9::SymbolReferenceTable::dynamicMethodSymrefsByCallSiteIndex(int32_t index)
   {
   if (!_dynamicMethodSymrefsByCallSiteIndex[index])
      _dynamicMethodSymrefsByCallSiteIndex[index] = new (trHeapMemory()) List<TR::SymbolReference>(comp()->trMemory());
   return _dynamicMethodSymrefsByCallSiteIndex[index];
   }

bool
J9::SymbolReferenceTable::isFieldClassObject(TR::SymbolReference *symRef)
   {
   int32_t len;
   const char *fieldName = symRef->getOwningMethod(comp())->fieldSignatureChars(symRef->getCPIndex(), len);
   dumpOptDetails(comp(), "got fieldsig as %.*s\n", len, fieldName);
   return false;
   }

static bool isSignatureTypeBool(const char *fieldSignature, int32_t len)
   {
   return len == 1 && fieldSignature[0] == 'Z';
   }

static bool isSignatureTypeChar(const char *fieldSignature, int32_t len)
   {
   return len == 1 && fieldSignature[0] == 'C';
   }

static bool isSignatureReturnTypeBool(const char *methodSignature, int32_t len)
   {
   TR_ASSERT(len > 1, "Method signature is unexpectedly short %d", len);
   // Method signature must end with ")Z" to have a boolean result type
   return len > 1 && (')' == methodSignature[len-2]) && ('Z' == methodSignature[len-1]);
   }

bool
J9::SymbolReferenceTable::isFieldTypeBool(TR::SymbolReference *symRef)
   {
   int32_t len;
   const char *fieldSignature = symRef->getOwningMethod(comp())->fieldSignatureChars(symRef->getCPIndex(), len);
   dumpOptDetails(comp(), "got field signature as %.*s\n", len, fieldSignature);
   return isSignatureTypeBool(fieldSignature, len);
   }

bool
J9::SymbolReferenceTable::isStaticTypeBool(TR::SymbolReference *symRef)
   {
   int32_t len;
   const char *fieldSignature = symRef->getOwningMethod(comp())->staticSignatureChars(symRef->getCPIndex(), len);
   dumpOptDetails(comp(), "got static signature as %.*s\n", len, fieldSignature);
   return isSignatureTypeBool(fieldSignature, len);
   }

bool
J9::SymbolReferenceTable::isFieldTypeChar(TR::SymbolReference *symRef)
   {
   int32_t len;
   const char *fieldSignature = symRef->getOwningMethod(comp())->fieldSignatureChars(symRef->getCPIndex(), len);
   dumpOptDetails(comp(), "got field signature as %.*s\n", len, fieldSignature);
   return isSignatureTypeChar(fieldSignature, len);
   }

bool
J9::SymbolReferenceTable::isStaticTypeChar(TR::SymbolReference *symRef)
   {
   int32_t len;
   const char *fieldSignature = symRef->getOwningMethod(comp())->staticSignatureChars(symRef->getCPIndex(), len);
   dumpOptDetails(comp(), "got static signature as %.*s\n", len, fieldSignature);
   return isSignatureTypeChar(fieldSignature, len);
   }


bool
J9::SymbolReferenceTable::isReturnTypeBool(TR::SymbolReference *symRef)
   {
   TR::Method *method = symRef->getSymbol()->castToResolvedMethodSymbol()->getMethod();
   char *methodSignature = method->signatureChars();
   const int32_t len = method->signatureLength();
   dumpOptDetails(comp(), "got method signature as %.*s\n", len, methodSignature);
   return isSignatureReturnTypeBool(methodSignature, len);
   }

static bool parmSlotCameFromExpandingAnArchetypeArgPlaceholder(int32_t slot, TR::ResolvedMethodSymbol *sym)
   {
   TR_ResolvedJ9Method *meth = (TR_ResolvedJ9Method *) sym->getResolvedMethod();
   if (meth->convertToMethod()->isArchetypeSpecimen())
      return slot >= meth->archetypeArgPlaceholderSlot();
   else
      return false;
   }


void
J9::SymbolReferenceTable::addParameters(TR::ResolvedMethodSymbol * methodSymbol)
   {
   mcount_t index = methodSymbol->getResolvedMethodIndex();
   methodSymbol->setParameterList();
   ListIterator<TR::ParameterSymbol> parms(&methodSymbol->getParameterList());
   for (TR::ParameterSymbol * p = parms.getFirst(); p; p = parms.getNext())
      {
      TR::KnownObjectTable::Index knownObjectIndex = methodSymbol->getKnownObjectIndexForParm(p->getOrdinal());
      TR::SymbolReference *symRef = NULL;
      if (knownObjectIndex == TR::KnownObjectTable::UNKNOWN)
         symRef = new (trHeapMemory()) TR::SymbolReference(self(), p, index, p->getSlot());
      else
         symRef = createTempSymRefWithKnownObject(p, index, p->getSlot(), knownObjectIndex);
      methodSymbol->setParmSymRef(p->getSlot(), symRef);
      if (!parmSlotCameFromExpandingAnArchetypeArgPlaceholder(p->getSlot(), methodSymbol))
         methodSymbol->getAutoSymRefs(p->getSlot()).add(symRef);
      }
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateStaticSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex, bool isStore)
   {
   TR_ResolvedMethod * owningMethod = owningMethodSymbol->getResolvedMethod();
   void * dataAddress;
   TR::DataType type = TR::NoType;
   bool isVolatile, isFinal, isPrivate, isUnresolvedInCP;
   bool resolved = owningMethod->staticAttributes(comp(), cpIndex, &dataAddress, &type, &isVolatile, &isFinal, &isPrivate, isStore, &isUnresolvedInCP);

   bool sharesSymbol = false;

   TR::StaticSymbol * sym = 0;
   TR::SymbolReference * symRef = findStaticSymbol(owningMethod, cpIndex, type);
   if (symRef)
      {
      if ((resolved && !symRef->isUnresolved()) ||
          (!resolved && symRef->isUnresolved() && owningMethod == symRef->getOwningMethod(comp())))
         {
         symRef->setHasBeenAccessedAtRuntime(isUnresolvedInCP ? TR_no: TR_maybe);
         if (shouldMarkBlockAsCold(owningMethod, isUnresolvedInCP))
            markBlockAsCold();

         return symRef;
         }

      sym = symRef->getSymbol()->castToStaticSymbol();
      sharesSymbol = true;
      }
   else
      {
      TR::Symbol::RecognizedField recognizedField = TR::Symbol::searchRecognizedField(comp(), owningMethod, cpIndex, true);
      if (recognizedField != TR::Symbol::UnknownField)
         sym = TR::StaticSymbol::createRecognized(trHeapMemory(), type, recognizedField);
      else
         sym = TR::StaticSymbol::create(trHeapMemory(), type);

      if (isVolatile)
         sym->setVolatile();
      if (isFinal)
         sym->setFinal();
      if (isPrivate)
         sym->setPrivate();
      }

   int32_t unresolvedIndex = resolved ? 0 : _numUnresolvedSymbols++;

   if (sharesSymbol)
      symRef->setReallySharesSymbol();

   symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), cpIndex, unresolvedIndex, TR::KnownObjectTable::UNKNOWN);

   checkUserField(symRef);

   if (sharesSymbol)
      symRef->setReallySharesSymbol();

   if (resolved)
      {
      sym->setStaticAddress(dataAddress);
      }
   else
      {
      symRef->setUnresolved();
      symRef->setCanGCandReturn();
      symRef->setCanGCandExcept();
      }

   symRef->setHasBeenAccessedAtRuntime(isUnresolvedInCP? TR_no : TR_maybe);

   if (type == TR::Address)
      aliasBuilder.addressStaticSymRefs().set(symRef->getReferenceNumber());
   else if (type == TR::Int32)
      aliasBuilder.intStaticSymRefs().set(symRef->getReferenceNumber());
   else
      aliasBuilder.nonIntPrimitiveStaticSymRefs().set(symRef->getReferenceNumber());

   if (shouldMarkBlockAsCold(owningMethod, isUnresolvedInCP))
      markBlockAsCold();

   return symRef;
   }


static struct N { const char * name; } pnames[] =
   {
   "java/",
   "javax/",
   "com/ibm/",
   "com/sun/"
   };


void
J9::SymbolReferenceTable::checkUserField(TR::SymbolReference *symRef)
   {
   static const char *userField = feGetEnv("TR_UserField");
   if (!userField)
      {
      // In the absence of further analysis, treat everything as user fields
      setHasUserField(true);
      return;
      }

   if ((!symRef->getSymbol()->isShadow() && (!symRef->getSymbol()->isStatic() || symRef->getSymbol()->isMethod() || symRef->getSymbol()->isConstObjectRef())) || symRef->getCPIndex() <= 0)
      return;

   int32_t length;
   char * name = symRef->getOwningMethod(comp())->classNameOfFieldOrStatic(symRef->getCPIndex(), length);

   if (name == NULL || length == 0)
      return;

   TR_ASSERT(sizeof(pnames)/sizeof(char *) == _numNonUserFieldClasses,"Size of package names array is not correct\n");
   int32_t i;
   bool isNonUserField = false;
   for (i = 0; i < _numNonUserFieldClasses; i++)
      {
      if (!strncmp(pnames[i].name, name, strlen(pnames[i].name)))
         {
         isNonUserField = true;
         //printf ("User field symref %d name=%s  length=%d cpindex=%d\n", symRef->getReferenceNumber(), name, length, symRef->getCPIndex());
         break;
         }
      }

   if (isNonUserField)
      {
      // At the moment this is conservative. i.e. any field in any of the above packages will not be considered
      // a user field in any of the other packages. However it is possible to have fields from one package (or class) that we
      // may want to consider as a user field in another package (class) in which case having the different array elements
      // below would then be the way to acomplish this. The population of the array would need to be more selective.
      //
      }
   else
      {
      setHasUserField(true);
      for (i = 0; i < _numNonUserFieldClasses; i++)
         aliasBuilder.userFieldSymRefNumbers()[i]->set(symRef->getReferenceNumber());
      }
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateThreadLowTenureAddressSymbolRef()
   {
   if (!element(lowTenureAddressSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "lowTenureAddress");
      sym->setDataType(TR::Address);
      sym->setNotCollected();
      element(lowTenureAddressSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), lowTenureAddressSymbol, sym);
      element(lowTenureAddressSymbol)->setOffset(fej9->getThreadLowTenureAddressPointerOffset());
      }
   return element(lowTenureAddressSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateThreadHighTenureAddressSymbolRef()
   {
   if (!element(highTenureAddressSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "highTenureAddress");
      sym->setDataType(TR::Address);
      sym->setNotCollected();
      element(highTenureAddressSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), highTenureAddressSymbol, sym);
      element(highTenureAddressSymbol)->setOffset(fej9->getThreadHighTenureAddressPointerOffset());
      }
   return element(highTenureAddressSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateRamStaticsFromClassSymbolRef()
   {
   if (!element(ramStaticsFromClassSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Address);
      element(ramStaticsFromClassSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), ramStaticsFromClassSymbol, sym);
      element(ramStaticsFromClassSymbol)->setOffset(fej9->getOffsetOfRamStaticsFromClassField());
      sym->setNotCollected();
      }
   return element(ramStaticsFromClassSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateJavaLangClassFromClassSymbolRef()
   {
   if (!element(javaLangClassFromClassSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Address);
      element(javaLangClassFromClassSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), javaLangClassFromClassSymbol, sym);
      element(javaLangClassFromClassSymbol)->setOffset(fej9->getOffsetOfJavaLangClassFromClassField());
      }
   return element(javaLangClassFromClassSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateClassFromJavaLangClassSymbolRef()
   {
   if (!element(classFromJavaLangClassSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Address);
      element(classFromJavaLangClassSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), classFromJavaLangClassSymbol, sym);
      element(classFromJavaLangClassSymbol)->setOffset(fej9->getOffsetOfClassFromJavaLangClassField());
      sym->setNotCollected();
      }
   return element(classFromJavaLangClassSymbol);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findInitializeStatusFromClassSymbolRef()
   {
   return element(initializeStatusFromClassSymbol);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateInitializeStatusFromClassSymbolRef()
   {
   if (!element(initializeStatusFromClassSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = NULL;
      if (self()->comp()->target().is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(initializeStatusFromClassSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), initializeStatusFromClassSymbol, sym);
      element(initializeStatusFromClassSymbol)->setOffset(fej9->getOffsetOfInitializeStatusFromClassField());
      }
   return element(initializeStatusFromClassSymbol);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateClassRomPtrSymbolRef()
   {
   if (!element(classRomPtrSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Address);
      element(classRomPtrSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), classRomPtrSymbol, sym);
      element(classRomPtrSymbol)->setOffset(fej9->getOffsetOfClassRomPtrField());
      if (!TR::Compiler->cls.romClassObjectsMayBeCollected())
         sym->setNotCollected();
      }
   return element(classRomPtrSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateGlobalFragmentSymbolRef()
   {
  if (!element(globalFragmentSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym;
      if (self()->comp()->target().is64Bit())
         sym = TR::Symbol::createShadow(trHeapMemory(),TR::Int64);
      else
         sym = TR::Symbol::createShadow(trHeapMemory(),TR::Int32);
      sym->setGlobalFragmentShadowSymbol();
      element(globalFragmentSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), globalFragmentSymbol, sym);
      element(globalFragmentSymbol)->setOffset(fej9->getRememberedSetGlobalFragmentOffset());
      }
   return element(globalFragmentSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateFragmentParentSymbolRef()
   {
   if (!element(fragmentParentSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "FragmentParent");
      sym->setDataType(TR::Address);
      sym->setNotCollected();
      element(fragmentParentSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), fragmentParentSymbol, sym);
      element(fragmentParentSymbol)->setOffset(fej9->thisThreadRememberedSetFragmentOffset() + fej9->getFragmentParentOffset());
      }
   return element(fragmentParentSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateThreadDebugEventData(int32_t index)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   intptr_t offset = fej9->getThreadDebugEventDataOffset(index);
   ListIterator<TR::SymbolReference> li(&_currentThreadDebugEventDataSymbolRefs);
   TR::SymbolReference * symRef;
   for (symRef = li.getFirst(); symRef; symRef = li.getNext())
      if (symRef->getOffset() == offset)
         return symRef;

   // Not found; create
   if (!_currentThreadDebugEventDataSymbol)
      {
      _currentThreadDebugEventDataSymbol = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "debugEventData");
      _currentThreadDebugEventDataSymbol->setDataType(TR::Address);
      _currentThreadDebugEventDataSymbol->setNotCollected();
      }
   TR::SymbolReference *result = new (trHeapMemory()) TR::SymbolReference(self(), _currentThreadDebugEventDataSymbol, offset);
   _currentThreadDebugEventDataSymbolRefs.add(result);
   return result;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findUnsafeSymbolRef(TR::DataType type, bool javaObjectReference, bool javaStaticReference, TR::Symbol::MemoryOrdering ordering)
   {
   TR_Array<TR::SymbolReference *> * unsafeSymRefs = NULL;

   unsafeSymRefs =
         javaStaticReference ?
               _unsafeJavaStaticSymRefs[static_cast<int>(ordering)] :
               _unsafeSymRefs[static_cast<int>(ordering)];


   TR::SymbolReference * symRef = NULL;

   if (unsafeSymRefs != NULL)
      {
      symRef = (*unsafeSymRefs)[type];
      }

   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateUnsafeSymbolRef(TR::DataType type, bool javaObjectReference, bool javaStaticReference, TR::Symbol::MemoryOrdering ordering)
   {
   TR_Array<TR::SymbolReference *> * unsafeSymRefs = NULL;

   if (javaStaticReference)
      {
      if (_unsafeJavaStaticSymRefs[static_cast<int>(ordering)] == NULL)
         _unsafeJavaStaticSymRefs[static_cast<int>(ordering)] = new (trHeapMemory()) TR_Array<TR::SymbolReference *>(comp()->trMemory(), TR::NumAllTypes);
      unsafeSymRefs = _unsafeJavaStaticSymRefs[static_cast<int>(ordering)];
      }
   else
      {
      if (_unsafeSymRefs[static_cast<int>(ordering)] == NULL)
         _unsafeSymRefs[static_cast<int>(ordering)] = new (trHeapMemory()) TR_Array<TR::SymbolReference *>(comp()->trMemory(), TR::NumAllTypes);
      unsafeSymRefs = _unsafeSymRefs[static_cast<int>(ordering)];
      }

   TR::SymbolReference * symRef = (*unsafeSymRefs)[type];

   if (symRef == NULL)
      {
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(),type);
      sym->setUnsafeShadowSymbol();
      sym->setArrayShadowSymbol();
      sym->setMemoryOrdering(ordering);
      (*unsafeSymRefs)[type] = symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, comp()->getMethodSymbol()->getResolvedMethodIndex(), -1);
      aliasBuilder.unsafeSymRefNumbers().set(symRef->getReferenceNumber());
      }

   // For unsafe symbols created by a call that contained an object and an offset we'll alias
   // the symbol against all shadow and statics. If the Unsafe symbol was created using only an
   // address then we won't alias against other Java objects.
   //
   if (javaObjectReference)
      comp()->setHasUnsafeSymbol();
   else
      symRef->setReallySharesSymbol();

   return symRef;
   }


void
J9::SymbolReferenceTable::performClassLookahead(TR_PersistentClassInfo *classInfo, TR_ResolvedMethod *method)
   {
   // Do not perform class lookahead when peeking (including recursive class lookahead)
   //
   ((TR_J9ByteCodeIlGenerator *)(comp()->getCurrentIlGenerator()))->performClassLookahead(classInfo);
   }


int32_t
J9::SymbolReferenceTable::userFieldMethodId(TR::MethodSymbol *methodSymbol)
   {
   static const char *userField = feGetEnv("TR_UserField");
   if (userField)
      {
      TR::RecognizedMethod method = methodSymbol->getRecognizedMethod();
      if (method == TR::java_util_HashMap_rehash)
         return J9::nonUser_java_util_HashMap_rehash - 1;
      else if (method == TR::java_util_HashMap_analyzeMap)
         return J9::nonUser_java_util_HashMap_analyzeMap - 1;
      else if (method == TR::java_util_HashMap_calculateCapacity)
         return J9::nonUser_java_util_HashMap_calculateCapacity - 1;
      else if (method == TR::java_util_HashMap_findNullKeyEntry)
         return J9::nonUser_java_util_HashMap_findNullKeyEntry - 1;
      }
   return -1;
   }


int32_t
J9::SymbolReferenceTable::immutableConstructorId(TR::MethodSymbol *methodSymbol)
   {
   TR::RecognizedMethod method = methodSymbol->getRecognizedMethod();
   switch (method)
      {
      case TR::java_lang_String_init_String:
      case TR::java_lang_String_init_String_char:
      case TR::java_lang_String_init_int_int_char_boolean:
         // All String constructors share the same ID
         method = TR::java_lang_String_init;
         break;
      default:
         break;
      }

   if (TR::java_lang_Boolean_init <= method && method <= TR::java_lang_String_init)
      return method - TR::java_lang_Boolean_init;
   else
      return -1;
   }


bool
J9::SymbolReferenceTable::isImmutable(TR::SymbolReference *symRef)
   {
   if (!_hasImmutable)
      return false;

   int32_t i;
   for (i = 0; i < _numImmutableClasses; i++)
      {
      if (_immutableSymRefNumbers[i]->get(symRef->getReferenceNumber()))
         return true;
      }

   ListElement<TR_ImmutableInfo> *immutableClassInfoElem = _immutableInfo.getListHead();
   while (immutableClassInfoElem)
      {
      TR_ImmutableInfo *immutableClassInfo = immutableClassInfoElem->getData();
      if (immutableClassInfo->_immutableSymRefNumbers->get(symRef->getReferenceNumber()))
         return true;
      immutableClassInfoElem = immutableClassInfoElem->getNextElement();
      }

   return false;
   }


TR_ImmutableInfo *
J9::SymbolReferenceTable::findOrCreateImmutableInfo(TR_OpaqueClassBlock *clazz)
   {
   ListElement<TR_ImmutableInfo> *immutableClassInfoElem = _immutableInfo.getListHead();
   while (immutableClassInfoElem)
      {
      TR_ImmutableInfo *immutableClassInfo = immutableClassInfoElem->getData();
      if (immutableClassInfo->_clazz == clazz)
         return immutableClassInfo;
      immutableClassInfoElem = immutableClassInfoElem->getNextElement();
      }

   TR_ImmutableInfo *newImmutableInfo = new (trHeapMemory()) TR_ImmutableInfo(clazz,
                                                             new (trHeapMemory()) TR_BitVector(_size_hint, comp()->trMemory(), heapAlloc, growable),
                                                             NULL);
   _immutableInfo.add(newImmutableInfo);
   return newImmutableInfo;
   }


void
J9::SymbolReferenceTable::checkImmutable(TR::SymbolReference *symRef)
   {
   if (!symRef->getSymbol()->isShadow() || symRef->getCPIndex() < 0)
      return;

   int32_t length;
   char * name = symRef->getOwningMethod(comp())->classNameOfFieldOrStatic(symRef->getCPIndex(), length);

   if (name == NULL || length == 0)
      return;

   static struct N { const char * name; } names[] =
      {
      "java/lang/Boolean",
      "java/lang/Character",
      "java/lang/Byte",
      "java/lang/Short",
      "java/lang/Integer",
      "java/lang/Long",
      "java/lang/Float",
      "java/lang/Double",
      "java/lang/String"
      };

   if (!comp()->getOption(TR_DisableImmutableFieldAliasing))
      {
      TR_ASSERT(sizeof(names)/sizeof(char *) == _numImmutableClasses,"Size of names array is not correct\n");
      int32_t i;
      for (i = 0; i < _numImmutableClasses; i++)
         {
         if (strcmp(names[i].name, name) == 0)
            {
            _hasImmutable = true;
            _immutableSymRefNumbers[i]->set(symRef->getReferenceNumber());
            break;
            }
         }
      }

   if (true)
      {
      if (!symRef->getSymbol()->isArrayShadowSymbol())
         {
         TR::Symbol *sym = symRef->getSymbol();
         if (sym->isPrivate() || sym->isFinal())
            {
            int32_t len;
            char *classNameOfFieldOrStatic = symRef->getOwningMethod(comp())->classNameOfFieldOrStatic(symRef->getCPIndex(), len);
            TR_OpaqueClassBlock * classOfStatic = comp()->fe()->getClassFromSignature(classNameOfFieldOrStatic, len, symRef->getOwningMethod(comp()));

            bool isClassInitialized = false;
            TR_PersistentClassInfo * classInfo =
            comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(classOfStatic, comp());
            if (classInfo && classInfo->isInitialized())
               isClassInitialized = true;

            if ((classOfStatic != comp()->getSystemClassPointer()) &&
                isClassInitialized && TR::Compiler->cls.isClassFinal(comp(), classOfStatic))
               {
               if (!classInfo->getFieldInfo() &&
                    (comp()->getMethodHotness() >= hot))
                  {
                  performClassLookahead(classInfo, symRef->getOwningMethod(comp()));
                  }

               if (classInfo->getFieldInfo())
                  {
                  TR_PersistentFieldInfo * fieldInfo = classInfo->getFieldInfo()->find(comp(), sym, symRef);
                  if (fieldInfo && fieldInfo->isImmutable())
                     {
                     setHasImmutable(true);
                     TR_ImmutableInfo *clazzImmutableInfo = findOrCreateImmutableInfo(classOfStatic);
                     clazzImmutableInfo->_immutableSymRefNumbers->set(symRef->getReferenceNumber());
                     }
                  }
               }
            }
         }
      }
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateImmutableGenericIntShadowSymbolReference(intptr_t offset)
   {
   static char *disableImmutableIntShadows = feGetEnv("TR_disableImmutableIntShadows");
   if (disableImmutableIntShadows)
      return findOrCreateGenericIntShadowSymbolReference(offset);
   TR::SymbolReference * symRef = new (trHeapMemory()) TR::SymbolReference(self(), findOrCreateGenericIntShadowSymbol(), comp()->getMethodSymbol()->getResolvedMethodIndex(), -1);
   symRef->setOffset(offset);
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateCheckCastForArrayStoreSymbolRef(TR::ResolvedMethodSymbol *)
   {
   return findOrCreateRuntimeHelper(TR_checkCastForArrayStore, false, true, true);
   }



TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateProfilingBufferCursorSymbolRef()
   {
   if (!element(profilingBufferCursorSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "ProfilingBufferCursor");
      sym->setDataType(TR::Address);
      element(profilingBufferCursorSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), profilingBufferCursorSymbol, sym);
      element(profilingBufferCursorSymbol)->setOffset(fej9->thisThreadGetProfilingBufferCursorOffset());

      // We can't let the load/store of the exception symbol swing down
      aliasBuilder.addressStaticSymRefs().set(getNonhelperIndex(profilingBufferCursorSymbol)); // add the symRef to the statics list to get correct aliasing info
      }
   return element(profilingBufferCursorSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateProfilingBufferEndSymbolRef()
   {
   if (!element(profilingBufferEndSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(),"profilingBufferEnd");
      sym->setDataType(TR::Address);
      element(profilingBufferEndSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), profilingBufferEndSymbol, sym);
      element(profilingBufferEndSymbol)->setOffset(fej9->thisThreadGetProfilingBufferEndOffset());

      // We can't let the load/store of the exception symbol swing down
      aliasBuilder.addressStaticSymRefs().set(getNonhelperIndex(profilingBufferEndSymbol)); // add the symRef to the statics list to get correct aliasing info
      }
   return element(profilingBufferEndSymbol);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateVMThreadTempSlotFieldSymbolRef()
   {
   if (!element(j9VMThreadTempSlotFieldSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "j9VMThreadTempSlotField");
      sym->setDataType(TR::Address);
      element(j9VMThreadTempSlotFieldSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), j9VMThreadTempSlotFieldSymbol, sym);
      element(j9VMThreadTempSlotFieldSymbol)->setOffset(fej9->thisThreadGetTempSlotOffset());
      aliasBuilder.addressStaticSymRefs().set(getNonhelperIndex(j9VMThreadTempSlotFieldSymbol));
      }
   return element(j9VMThreadTempSlotFieldSymbol);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateProfilingBufferSymbolRef(intptr_t offset)
   {
   if (!element(profilingBufferSymbol))
      {
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory());
      sym->setDataType(TR::Int32);
      TR::SymbolReference *symRef = new (trHeapMemory()) TR::SymbolReference(self(), profilingBufferSymbol, sym);
      element(profilingBufferSymbol) = symRef;
      element(profilingBufferSymbol)->setOffset(offset);
      }
   return element(profilingBufferSymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findShadowSymbol(TR_ResolvedMethod * owningMethod, int32_t cpIndex, TR::DataType type, TR::Symbol::RecognizedField *recognizedField)
   {
   TR::SymbolReference * symRef;
   TR_SymRefIterator i(type == TR::Address ? aliasBuilder.addressShadowSymRefs() :
                                            (type == TR::Int32 ? aliasBuilder.intShadowSymRefs() : aliasBuilder.nonIntPrimitiveShadowSymRefs()), self());
   while ((symRef = i.getNext()) != NULL)
      {
      if ((recognizedField &&
          *recognizedField != TR::Symbol::UnknownField &&
          *recognizedField == symRef->getSymbol()->getRecognizedField())||
          (symRef->getSymbol()->getDataType() == type &&
          symRef->getCPIndex() != -1 &&
          cpIndex != -1 &&
          TR::Compiler->cls.jitFieldsAreSame(comp(), owningMethod, cpIndex,
             symRef->getOwningMethod(comp()), symRef->getCPIndex(), symRef->getSymbol()->isStatic())))
         {
         if (cpIndex != -1 && owningMethod->classOfMethod() != symRef->getOwningMethod(comp())->classOfMethod())
            {
            bool isVolatile = true, isFinal = false, isPrivate = false, isUnresolvedInCP ;
            TR::DataType type = TR::DataTypes::NoType;
            uint32_t offset = 0;
            // isStore is a hardcoded false here since we only really want to set hasBeenAccessed correctly
            bool resolved = owningMethod->fieldAttributes(comp(), cpIndex, &offset, &type, &isVolatile, &isFinal, &isPrivate, false, &isUnresolvedInCP, true);
            symRef->setHasBeenAccessedAtRuntime(isUnresolvedInCP ? TR_no : TR_maybe);
            }
         return symRef;
         }
      }
   return 0;
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateClassIsArraySymbolRef()
   {
   if (!element(isArraySymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Int32);
      element(isArraySymbol) = new (trHeapMemory()) TR::SymbolReference(self(), isArraySymbol, sym);
      element(isArraySymbol)->setOffset(fej9->getOffsetOfIsArrayFieldFromRomClass());
      }
   return element(isArraySymbol);
   }


TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateArrayComponentTypeSymbolRef()
   {
   if (!element(componentClassSymbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), TR::Address);
      element(componentClassSymbol) = new (trHeapMemory()) TR::SymbolReference(self(), componentClassSymbol, sym);
      element(componentClassSymbol)->setOffset(fej9->getOffsetOfArrayComponentTypeField());
      if (!TR::Compiler->cls.classObjectsMayBeCollected())
         sym->setNotCollected();
      }
   return element(componentClassSymbol);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateVMThreadFloatTemp1SymbolRef()
   {
   if (!element(j9VMThreadFloatTemp1Symbol))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      TR::Symbol * sym = TR::RegisterMappedSymbol::createMethodMetaDataSymbol(trHeapMemory(), "j9VMThreadFloatTemp1");
      sym->setDataType(TR::Address);
      element(j9VMThreadFloatTemp1Symbol) = new (trHeapMemory()) TR::SymbolReference(self(), j9VMThreadFloatTemp1Symbol, sym);
      element(j9VMThreadFloatTemp1Symbol)->setOffset(fej9->thisThreadGetFloatTemp1Offset());
      aliasBuilder.addressStaticSymRefs().set(getNonhelperIndex(j9VMThreadFloatTemp1Symbol));
      }
   return element(j9VMThreadFloatTemp1Symbol);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateObjectInequalityComparisonSymbolRef()
   {
   TR::SymbolReference *symRef = element(objectInequalityComparisonSymbol);
   if (symRef != NULL)
      return symRef;

   symRef = self()->findOrCreateCodeGenInlinedHelper(objectInequalityComparisonSymbol);
   symRef->setCanGCandReturn();
   symRef->setCanGCandExcept();
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateObjectEqualityComparisonSymbolRef()
   {
   TR::SymbolReference *symRef = element(objectEqualityComparisonSymbol);
   if (symRef != NULL)
      return symRef;

   symRef = self()->findOrCreateCodeGenInlinedHelper(objectEqualityComparisonSymbol);
   symRef->setCanGCandReturn();
   symRef->setCanGCandExcept();
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateEncodeASCIISymbolRef()
   {
   TR::SymbolReference *symRef = element(encodeASCIISymbol);
   if (symRef != NULL)
      return symRef;

   symRef = self()->findOrCreateCodeGenInlinedHelper(encodeASCIISymbol);
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateNonNullableArrayNullStoreCheckSymbolRef()
   {
   TR::SymbolReference *symRef = element(nonNullableArrayNullStoreCheckSymbol);
   if (symRef != NULL)
      return symRef;

   symRef = self()->findOrCreateCodeGenInlinedHelper(nonNullableArrayNullStoreCheckSymbol);
   symRef->setCanGCandExcept();
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateLoadFlattenableArrayElementNonHelperSymbolRef()
   {
   TR::SymbolReference *symRef = element(loadFlattenableArrayElementNonHelperSymbol);
   if (symRef != NULL)
      return symRef;

   symRef = self()->findOrCreateCodeGenInlinedHelper(loadFlattenableArrayElementNonHelperSymbol);
   symRef->setCanGCandExcept();
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateStoreFlattenableArrayElementNonHelperSymbolRef()
   {
   TR::SymbolReference *symRef = element(storeFlattenableArrayElementNonHelperSymbol);
   if (symRef != NULL)
      return symRef;

   symRef = self()->findOrCreateCodeGenInlinedHelper(storeFlattenableArrayElementNonHelperSymbol);
   symRef->setCanGCandExcept();
   return symRef;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findOrCreateIsIdentityObjectNonHelperSymbolRef()
   {
   TR::SymbolReference *symRef = element(isIdentityObjectNonHelperSymbol);
   if (symRef != NULL)
      {
      return symRef;
      }

   symRef = self()->findOrCreateCodeGenInlinedHelper(isIdentityObjectNonHelperSymbol);
   return symRef;
   }

TR::ParameterSymbol *
J9::SymbolReferenceTable::createParameterSymbol(
      TR::ResolvedMethodSymbol *owningMethodSymbol,
      int32_t slot,
      TR::DataType type,
      TR::KnownObjectTable::Index knownObjectIndex)
   {
   TR::ParameterSymbol * sym = TR::ParameterSymbol::create(trHeapMemory(),type,slot);

   if (comp()->getOption(TR_MimicInterpreterFrameShape))
      {
      int32_t parameterSlots = owningMethodSymbol->getNumParameterSlots();
      sym->setGCMapIndex(-slot + parameterSlots - sym->getNumberOfSlots());
      }

   TR::SymbolReference *symRef = NULL;
   if (knownObjectIndex == TR::KnownObjectTable::UNKNOWN)
      symRef = new (trHeapMemory()) TR::SymbolReference(self(), sym, owningMethodSymbol->getResolvedMethodIndex(), slot);
   else
      symRef = createTempSymRefWithKnownObject(sym,  owningMethodSymbol->getResolvedMethodIndex(), slot, knownObjectIndex);

   owningMethodSymbol->setParmSymRef(slot, symRef);
   if (!parmSlotCameFromExpandingAnArchetypeArgPlaceholder(slot, owningMethodSymbol))
      owningMethodSymbol->getAutoSymRefs(slot).add(symRef);

   return sym;
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findArrayClassRomPtrSymbolRef()
   {
   return element(arrayClassRomPtrSymbol);
   }

TR::SymbolReference *
J9::SymbolReferenceTable::findClassRomPtrSymbolRef()
   {
   return element(classRomPtrSymbol);
   }

const char *J9::SymbolReferenceTable::_commonNonHelperSymbolNames[] =
   {
   "<classRomPtr>",
   "<ramStaticsFromClass>",
   "<componentClassAsPrimitive>",
   "<initializeStatusFromClass>",
   "<threadPrivateFlags>",
   "<arrayletSpineFirstElement>",
   "<dltBlock>",
   "<classFromJavaLangClassAsPrimitive>",
   "<javaVM>",
   "<j9methodExtraField>",
   "<j9methodConstantPoolField>",
   "<startPCLinkageInfo>",
   "<instanceShapeFromROMClass>",
   "<j9VMThreadTempSlotField>"
   };


const char *
J9::SymbolReferenceTable::getNonHelperSymbolName(CommonNonhelperSymbol nonHelper)
   {
#ifdef OMR_ENABLE_NONHELPER_EXTENSIBLE_ENUM
   TR_ASSERT_FATAL(nonHelper <= J9lastNonhelperSymbol, "unknown nonhelper %" OMR_PRId32, static_cast<int32_t>(nonHelper));

   static_assert(sizeof(_commonNonHelperSymbolNames)/sizeof(_commonNonHelperSymbolNames[0]) ==
                 static_cast<int32_t>(J9lastNonhelperSymbol - J9firstNonhelperSymbol + 1),
              "_commonNonHelperSymbolNames array must match CommonNonHelperSymbol enumeration");

   if (nonHelper >= J9firstNonhelperSymbol)
      {
      return _commonNonHelperSymbolNames[static_cast<int32_t>(nonHelper - J9firstNonhelperSymbol)];
      }
   else
      {
      return OMR::SymbolReferenceTableConnector::getNonHelperSymbolName(nonHelper);
      }
#else
   return NULL;
#endif
   }
