# Copyright (c) 2024 Marcos Raimondi
# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_SOPHGO_CV180X_IPM ipm_cv180x.c)
