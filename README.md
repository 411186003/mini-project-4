Encoder.c
將文字檔輸入進行 Huffman 編碼，輸出：
編碼檔（encoded.bin）
Huffman codebook（codebook.csv）
運行 log（encoder.log）

Decoder.c
使用 codebook 將編碼檔還原成文字檔，輸出：
解碼文字檔（output.txt）
運行 log（decoder.log）

logger.c/h
提供統一的 log 功能，用於記錄編碼與解碼過程。

input.txt
用來測試encoder/decoder是否正確。

.github/workflows/c_build-simple.yml：
在每次 push 到 main 分支時，自動編譯 encoder/decoder、執行編碼解碼、上傳產物，並檢查輸入與輸出是否一致。
可以快速驗證程式更新後的正確性。

.github/workflows/c_build-complex.yml:
同上，但要使用curl下載input.txt。

產物 (Artifacts)
Encoder: encoded.bin、codebook.csv、encoder.log
Decoder: output.txt、decoder.log


工作分配

411186003 王麒禎 完成encoder.c decoder.c

411186008 林子陽 負責debug,解決"沒有成功編碼的原因

411186016 葉哲旭 encoder.c decoder.c 的主架構,workflows製作


