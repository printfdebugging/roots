
#ifndef __has_feature
// GCC does not have __has_feature...
#define __has_feature(feature) 0
#endif

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
const char *__lsan_default_options()
{
   return "print_suppressions=1";
}

const char *__lsan_default_suppressions()
{
   return "leak:_glfwConnectX11\n"
          "leak:_glfwInitX11\n"
          "leak:getSystemContentScale\n"
          "leak:extensionSupportedGLX\n"
          "leak:_glfwInitGLX\n"
          "leak:_glfwCreateWindowX11\n"
          "leak:glfwCreateWindow\n";
}
#endif
