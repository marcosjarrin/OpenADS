# tiny-AES-c

Vendored from https://github.com/kokke/tiny-AES-c (commit at the URL fetch time).

Licence: Unlicense (public domain, see `LICENSE`).

OpenADS uses AES-128 and AES-256 in ECB mode for record-level encryption,
matching ADS legacy behaviour. The library also supports CBC and CTR
modes which OpenADS does not currently use.

Update procedure: replace `aes.h` and `aes.c` with the new release files,
re-run the encryption unit tests.
