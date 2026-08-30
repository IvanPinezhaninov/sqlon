/******************************************************************************
**
** Copyright (C) 2026 Ivan Pinezhaninov <ivan.pinezhaninov@gmail.com>
**
** This file is part of the SQLon - which can be found at
** https://github.com/IvanPinezhaninov/sqlon/.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
** DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
** OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
** THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**
******************************************************************************/

#ifndef SQLON_EXPORT_H
#define SQLON_EXPORT_H

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(SQLON_STATIC)
#define SQLON_API
#elif defined(SQLON_BUILDING_LIBRARY)
#define SQLON_API __declspec(dllexport)
#else
#define SQLON_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define SQLON_API __attribute__((visibility("default")))
#else
#define SQLON_API
#endif

#endif // SQLON_EXPORT_H
