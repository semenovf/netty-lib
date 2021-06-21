#pragma once
#ifndef DLL_API
#   if defined(_WIN32) || defined(_WIN64)
#       if defined(DLL_EXPORTS)
#           define DLL_API __declspec(dllexport)
#       else
#           define DLL_API __declspec(dllimport)
#       endif
#   else
#       define DLL_API
#   endif
#endif // !DLL_API
