/*
 * Copyright IBM Corp. and others 2026
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
 */
package org.openj9.test;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * LibertyWorkload - Generates HTTP requests against a Liberty server
 * to exercise JFR event recording in a real application server context.
 */
public class LibertyWorkload {
	private static final int DEFAULT_REQUESTS = 100;
	private static final int DEFAULT_THREADS = 5;
	private static final long REQUEST_DELAY_MS = 100;

	private final String baseUrl;
	private final int totalRequests;
	private final int numThreads;
	private final AtomicInteger successCount = new AtomicInteger(0);
	private final AtomicInteger failureCount = new AtomicInteger(0);

	public LibertyWorkload(String baseUrl, int totalRequests, int numThreads) {
		this.baseUrl = baseUrl;
		this.totalRequests = totalRequests;
		this.numThreads = numThreads;
	}

	public static void main(String[] args) {
		if (args.length < 1) {
			System.err.println("Usage: LibertyWorkload <baseUrl> [requests] [threads]");
			System.err.println("Example: LibertyWorkload http://localhost:9080 100 5");
			System.exit(1);
		}

		String baseUrl = args[0];
		int requests = (args.length > 1) ? Integer.parseInt(args[1]) : DEFAULT_REQUESTS;
		int threads = (args.length > 2) ? Integer.parseInt(args[2]) : DEFAULT_THREADS;

		System.out.println("Starting Liberty workload:");
		System.out.println("  Base URL: " + baseUrl);
		System.out.println("  Total requests: " + requests);
		System.out.println("  Concurrent threads: " + threads);

		LibertyWorkload workload = new LibertyWorkload(baseUrl, requests, threads);
		workload.run();
	}

	public void run() {
		long startTime = System.currentTimeMillis();

		// Wait for Liberty to be ready
		if (!waitForServerReady()) {
			System.err.println("ERROR: Liberty server not ready");
			System.exit(1);
		}

		// Create worker threads
		Thread[] workers = new Thread[numThreads];
		int requestsPerThread = totalRequests / numThreads;

		for (int i = 0; i < numThreads; i++) {
			final int threadId = i;
			final int threadRequests = (i == numThreads - 1)
				? requestsPerThread + (totalRequests % numThreads)
				: requestsPerThread;

			workers[i] = new Thread(() -> executeRequests(threadId, threadRequests));
			workers[i].setName("WorkloadThread-" + threadId);
			workers[i].start();
		}

		// Wait for all threads to complete
		for (Thread worker : workers) {
			try {
				worker.join();
			} catch (InterruptedException e) {
				System.err.println("Thread interrupted: " + e.getMessage());
			}
		}

		long duration = System.currentTimeMillis() - startTime;

		System.out.println("\nWorkload complete:");
		System.out.println("  Successful requests: " + successCount.get());
		System.out.println("  Failed requests: " + failureCount.get());
		System.out.println("  Duration: " + duration + " ms");
		System.out.println("  Throughput: " + (totalRequests * 1000.0 / duration) + " req/s");

		if (failureCount.get() > 0) {
			System.err.println("WARNING: Some requests failed");
		}
	}

	private boolean waitForServerReady() {
		System.out.println("Waiting for Liberty server to be ready...");
		int maxAttempts = 30;
		int attempt = 0;

		while (attempt < maxAttempts) {
			try {
				URL url = new URL(baseUrl);
				HttpURLConnection conn = (HttpURLConnection) url.openConnection();
				conn.setConnectTimeout(1000);
				conn.setReadTimeout(1000);
				conn.setRequestMethod("GET");

				int responseCode = conn.getResponseCode();
				conn.disconnect();

				if (responseCode > 0) {
					System.out.println("Liberty server is ready (response code: " + responseCode + ")");
					return true;
				}
			} catch (Exception e) {
				// Server not ready yet, continue waiting
			}

			attempt++;
			try {
				Thread.sleep(1000);
			} catch (InterruptedException e) {
				return false;
			}
		}

		return false;
	}

	private void executeRequests(int threadId, int numRequests) {
		for (int i = 0; i < numRequests; i++) {
			try {
				boolean success = sendRequest(threadId, i);
				if (success) {
					successCount.incrementAndGet();
				} else {
					failureCount.incrementAndGet();
				}

				// Add delay between requests
				if (i < numRequests - 1) {
					Thread.sleep(REQUEST_DELAY_MS);
				}
			} catch (InterruptedException e) {
				System.err.println("Thread " + threadId + " interrupted");
				break;
			}
		}
	}

	private boolean sendRequest(int threadId, int requestNum) {
		try {
			URL url = new URL(baseUrl);
			HttpURLConnection conn = (HttpURLConnection) url.openConnection();
			conn.setRequestMethod("GET");
			conn.setConnectTimeout(5000);
			conn.setReadTimeout(5000);
			conn.setRequestProperty("User-Agent", "LibertyWorkload-Thread-" + threadId);

			int responseCode = conn.getResponseCode();

			// Read response to ensure full request processing
			try (BufferedReader reader = new BufferedReader(
					new InputStreamReader(conn.getInputStream()))) {
				while (reader.readLine() != null) {
					// Consume response
				}
			} catch (Exception e) {
				// Ignore read errors
			}

			conn.disconnect();

			// Log progress every 10 requests
			if (requestNum % 10 == 0) {
				System.out.println("Thread " + threadId + ": Request " + requestNum
					+ " completed (HTTP " + responseCode + ")");
			}

			return (responseCode >= 200 && responseCode < 400);

		} catch (Exception e) {
			System.err.println("Thread " + threadId + ": Request " + requestNum
				+ " failed: " + e.getMessage());
			return false;
		}
	}
}

// Made with Bob
