# Intro

As the name suggests, this is a hugepage based arena allocator, or a bump allocator. It supports hugepages, transparent hugepages as well as 1GB hugepages.

This allocator is specifically for unix/ POSIX compliant systems- the windows calls for requesting memory are different. Further, this is only gcc and clang compliant. And not MSVC.

