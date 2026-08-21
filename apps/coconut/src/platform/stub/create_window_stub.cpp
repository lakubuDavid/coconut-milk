/// Stub: create_window and bundle detection functions.
/// These are extern "C" symbols normally defined in .mm files.

#ifdef NO_PLATFORM

#ifdef __cplusplus
extern "C" {
#endif

void* coconut_create_frameless_window(int x, int y, int w, int h) {
  (void)x; (void)y; (void)w; (void)h;
  return nullptr;
}

void* coconut_create_standard_window(int x, int y, int w, int h) {
  (void)x; (void)y; (void)w; (void)h;
  return nullptr;
}

const char* coconut_bundle_resource_path() {
  return nullptr;
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NO_PLATFORM
