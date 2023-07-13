This contains a data logging and tracing backend based on pwtrace

Documentation is available [here](https://pigweed.dev/pw_trace/).

## Usage

Implementation currently relies on tokenization using
[pw_tokenizer](https://pigweed.dev/pw_tokenizer/) to allow the usage of the
library on embedded devices (faster data transmission).

### Enabling

To enable setup the following `gn` arguments:

```
matter_enable_tracing_support = true
matter_trace_config = "${chip_root}/src/tracing/pwtrace:pwtrace_tracing"
```

Additionally you will need to set backends for:
  - `pw_assert`, typically to `$dir_pw_assert_tokenized` (since tokenization used)
  - `pw_log`, typically to `$dir_pw_log_tokenized` (since tokenization used)

```
import("//build_overrides/pigweed.gni")
# ...
pw_assert_BACKEND = "$dir_pw_assert_tokenized"
pw_log_BACKEND = "$dir_pw_log_tokenized"
pw_trace_BACKEND = "$dir_pw_trace_tokenized"
```

For tokenization to take effect, the following has to be added
to `pw_build_LINK_DEPS`:

```
pw_build_LINK_DEPS = [
   "$dir_pw_assert:impl",
   "$dir_pw_log:impl",
]
```

