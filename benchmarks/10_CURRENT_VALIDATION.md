# Current implementation validation — 2026-09-30

The reports numbered 00–09 retain their original measurement dates and metrics as historical evidence. Their PASS labels do not certify the current working tree. Fresh build, test, benchmark and TurboQuant ground-truth output are recorded separately below; no production quality claims follow from microbenchmark throughput.

| Request items | Resolution / evidence scope |
|---|---|
| 1, 19 | `ResourcePolicy.hpp` defines the RAM ceiling from detected physical RAM. Lower settings remain valid; settings above the default ceiling are clamped. The recorded 14,117 MiB is a legacy measurement inconsistent with the stated 31.2 GiB host; it is not the current 50% default. Governor pressure, admission, resident-loader budgeting and dynamic KV sizing now use the policy. |
| 2, 3 | Compute threads are bounded by half of hardware concurrency, with one thread minimum. Deterministic tests verify the four-thread reference host yields two threads, including when four are explicitly requested. This does not bound combined HTTP/OpenMP/other threads or total host CPU utilization. |
| 4 | Linux/WSL2 retain `/proc`. |
| 5, 22 | README identifies the two-core/four-thread reference host. RAM requirements depend on all resident models and KV dimensions; 16 GiB gives approximately 8 GiB policy headroom before process overhead. Production 1.5B resident-pool fit remains unvalidated. |
| 6 | Existing 64K allocation/persistence evidence uses a synthetic one-layer, 128-dimension model. Production Llama/DeepSeek memory, latency and quality at 64K remain unbenchmarked. No suitable GGUF production weights are available in this checkout. |
| 7 | Completion-policy evaluation speed is labeled as throughput, with hallucination-prevention effectiveness unmeasured. |
| 8 | Audio and image prototype timings are explicitly synthetic. Audio benchmark uses constant PCM without a Whisper model/decoder; no transcription quality or real-time transcription claim is supported. |
| 9, 10, 15 | Index references identify TurboQuant. Exhaustive quantized scoring covers all candidates but can change FP32 ranking. Retrieved history is bounded and lossy, not infinite context. |
| 11 | `test_turboquant` now compares recall@10 and mean FP32 cosine loss with FP32 ground truth at 128, 512 and 1536 dimensions, 1,000 vectors, 40 mixed random/near-neighbor queries, fixed seed 20260930. Assertions remain enabled in Release. This synthetic corpus is not a production embedding accuracy evaluation. |
| 12 | TurboQuant remains O(N); a sub-linear tier is not implemented. The existing index API still uses exhaustive scoring. Production corpus scale and latency need validation before selecting a partitioned or ANN architecture. |
| 13, 14 | Native forward pass checks tensor compatibility before matrix kernels. Errors include actual tensor names and types; inspection exposes `native_tensor_compatible` and `unsupported_native_tensors` separately from valid GGUF metadata. Q4_0/Q8_0 matrices and FP32 normalization/bias tensors are supported. Architecture support is a separate limitation. |
| 16 | Assessment uses “no external ML runtime dependencies.” |
| 17, 18 | Historical reports are explicitly superseded as current-certification evidence. Fresh results accompany this working-tree revision. |
| 20 | New settings/database seeds use `server.threads=0` for auto. Existing user settings are preserved and still clamped downstream. |
| 21 | The 512-dimension score measures the standalone FP32 pairwise cosine helper, not TurboQuant index retrieval. The dedicated TurboQuant throughput example scans 5,000 vectors at 1536 dimensions; its previous 50K output label is corrected. |

Fresh evidence ([manifest](results/2026-09-30/manifest.json)) records the base commit and a SHA-256 of the modified source tree; this is working-tree evidence, not a certificate for an unchanged commit.

- Full build completed; the final benchmark target rebuild also succeeded ([build log](results/2026-09-30/build.log)).
- 17/17 CTest suites passed in 36.12 seconds ([CTest log](results/2026-09-30/ctest.log)). The governor was rerun after the final overflow assertion was added ([governor log](results/2026-09-30/governor.log)). Tests that require absent production weights do not establish production inference coverage. Assertions are explicitly enabled in the two changed suites; other Release suites retain their existing configuration.
- Synthetic TurboQuant recall@10: **0.8775 / 0.9025 / 0.8950** at **128 / 512 / 1536 dimensions**. Mean FP32 cosine ranking loss: **0.00114909 / 0.000557386 / 0.000290743** ([ground-truth log](results/2026-09-30/turboquant.log)). Recall thresholds are regression bounds for this fixed synthetic corpus, not a production-quality guarantee.
- Final benchmark completed without concurrent build/test activity ([raw benchmark log](results/2026-09-30/benchmark.log)). Detected host RAM: **31,371 MiB**, default ceiling: **15,685 MiB**, compute threads: **2**. This live measurement replaces reliance on the inconsistent historical RAM figures.
- Synthetic constant-audio processing: **592 chunks/sec**; no real transcription result. KV serialization: **1,992.0 MiB/s**; eviction/allocation/warm-file restore: **907.7 MiB/s**, **17.63 ms per cycle**. The logger prints MB/s while dividing bytes by 1024², so these units are MiB/s.
- Vulkan was unavailable in this run; GPU measurements are not revalidated. Other microbenchmark rates remain workload- and compiler-dependent; they do not establish end-to-end latency, quality, or production reliability.

The fresh benchmark exposed an additional evidence bug: the old KV restore loop repeatedly returned the already resident session. The benchmark now evicts the session before every restore and checks save/restore success. Restore timings include eviction, allocation and OS-cached file reads, rather than claiming cold physical-disk bandwidth.
