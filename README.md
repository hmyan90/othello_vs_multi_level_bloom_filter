# othello_vs_multi_level_bloom_filter

The data in certificate_data directory is generated from https://github.com/hmyan90/process-certificate-revocation-data.

Compile with `make`, output is execuatable file `test`

Run with `./test certificate_data/final_revoked.json certificate_data/final_unrevoked.json ` to get all performance result.
