# OpenEmbedded layer

This layer packages the EasyTier extension for OneKVM. The integrating distro
must set `ONEKVM_COMPONENT_SRCREV`; the EasyTier upstream revision remains
independently pinned. The layer supports `wrynose` and depends on `onekvm`.
