FROM docker.io/devkitpro/devkita64:20260219@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

ARG OUNCE_GPG_URL="https://pkg.ouncebrew.org/ounce.gpg"
ARG OUNCE_GPG_FPR="DFEAE7FA965D1E6C0D41D4D75DB9FDBFA4A4DE29"
ARG LIBOUNCE_VER="20261123-1"
ARG LIBOUNCE_PKG="libounce-20261123-1-any.pkg.tar.zst"
ARG LIBOUNCE_SHA256="16cb51c8c83a461fec8e88a906e8ac3d0753e3924cdc82668ef861624c0252dc"
ARG LIBKAIHEN_VER="20261123-1"
ARG LIBKAIHEN_PKG="libkaihen-20261123-1-any.pkg.tar.zst"
ARG LIBKAIHEN_SHA256="7d6d615138f5d02faa4f46d2dd3071290f84d56908a5278e90040efd10020a6f"

RUN set -euo pipefail \
    && apt-get update -qq \
    && apt-get install -y -qq --no-install-recommends curl ca-certificates gnupg make \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /tmp/ounce-install \
    && mkdir -p -m 700 /tmp/ounce-install/gnupg \
    && curl -fsSL "${OUNCE_GPG_URL}" -o /tmp/ounce-install/ounce.gpg \
    && gpg --batch --with-colons --show-keys /tmp/ounce-install/ounce.gpg \
        | grep -Fx "fpr:::::::::${OUNCE_GPG_FPR}:" \
    && dkp-pacman-key --add /tmp/ounce-install/ounce.gpg \
    && dkp-pacman-key --lsign-key "${OUNCE_GPG_FPR}" \
    && printf '\n[ounce]\nServer = https://pkg.ouncebrew.org\n' >> /opt/devkitpro/pacman/etc/pacman.conf \
    && dkp-pacman -Sy --noconfirm \
    && curl -fsSL "https://pkg.ouncebrew.org/${LIBOUNCE_PKG}" -o "/tmp/ounce-install/${LIBOUNCE_PKG}" \
    && curl -fsSL "https://pkg.ouncebrew.org/${LIBOUNCE_PKG}.sig" -o "/tmp/ounce-install/${LIBOUNCE_PKG}.sig" \
    && curl -fsSL "https://pkg.ouncebrew.org/${LIBKAIHEN_PKG}" -o "/tmp/ounce-install/${LIBKAIHEN_PKG}" \
    && curl -fsSL "https://pkg.ouncebrew.org/${LIBKAIHEN_PKG}.sig" -o "/tmp/ounce-install/${LIBKAIHEN_PKG}.sig" \
    && echo "${LIBOUNCE_SHA256}  /tmp/ounce-install/${LIBOUNCE_PKG}" | sha256sum -c - \
    && echo "${LIBKAIHEN_SHA256}  /tmp/ounce-install/${LIBKAIHEN_PKG}" | sha256sum -c - \
    && gpg --homedir /tmp/ounce-install/gnupg --no-default-keyring --import /tmp/ounce-install/ounce.gpg \
    && gpg --homedir /tmp/ounce-install/gnupg --verify "/tmp/ounce-install/${LIBOUNCE_PKG}.sig" "/tmp/ounce-install/${LIBOUNCE_PKG}" \
    && gpg --homedir /tmp/ounce-install/gnupg --verify "/tmp/ounce-install/${LIBKAIHEN_PKG}.sig" "/tmp/ounce-install/${LIBKAIHEN_PKG}" \
    && dkp-pacman -U --noconfirm --overwrite '/opt/devkitpro/*' \
        "/tmp/ounce-install/${LIBOUNCE_PKG}" \
        "/tmp/ounce-install/${LIBKAIHEN_PKG}" \
    && test "$(dkp-pacman -Q libounce)" = "libounce ${LIBOUNCE_VER}" \
    && test "$(dkp-pacman -Q libkaihen)" = "libkaihen ${LIBKAIHEN_VER}" \
    && test "$(dkp-pacman -Q switch-freetype)" = "switch-freetype 2.13.3-1" \
    && test "$(dkp-pacman -Q switch-harfbuzz)" = "switch-harfbuzz 10.0.1-1" \
    && test "$(dkp-pacman -Q switch-libpng)" = "switch-libpng 1.6.48-1" \
    && test "$(dkp-pacman -Q switch-zlib)" = "switch-zlib 1.3.1-1" \
    && test "$(dkp-pacman -Q switch-bzip2)" = "switch-bzip2 1.0.8-1" \
    && rm -rf /tmp/ounce-install

WORKDIR /work
