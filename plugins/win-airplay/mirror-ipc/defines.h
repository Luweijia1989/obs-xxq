#ifdef MIRROR_IPC_LIB
#define MIRROR_IPC_LIB_EXPORT_HELPER __declspec(dllexport)
#else
#define MIRROR_IPC_LIB_EXPORT_HELPER __declspec(dllimport)
#endif
