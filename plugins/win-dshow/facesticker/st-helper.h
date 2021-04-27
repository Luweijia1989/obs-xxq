#ifndef ST_MOBILE_INCLUDE_HELPER_H_
#define ST_MOBILE_INCLUDE_HELPER_H_
#include <string.h>

const char *face_model = "M_SenseME_Face_Video_5.3.4.model";
const char *face_282_model = "M_SenseME_Face_Extra_5.23.4.model";
const char *mouthocc_model = "M_SenseME_MouthOcclusion_1.1.1.model";

const char *hand_model = "M_SenseME_Hand_6.0.8.model";
const char *segment_model = "M_SenseME_Segment_4.12.8.model";
const char *body14_model = "M_SenseME_Body_5.9.0.model";
const char *body_contour_model = "M_SenseME_Body_Contour_77_1.2.2.model";
const char *face_extra_model = "M_SenseME_Face_Extra_5.23.4.model";
const char *eyeball_model = "M_SenseME_Iris_2.0.0.model";
const char *ear_model = "M_Align_Deepface_Ear_1.1.1.model";
const char *mouth_parse_model = "M_SenseME_MouthOcclusion_1.0.0.model";
const char *body4_model = "M_SenseME_Body_Four_1.0.0.model";
const char *body8_model = "body8.model";
const char *hair_model = "M_SenseME_Segment_Hair_1.3.4.model";
const char *tongue_model = "M_Align_DeepFace_Tongue_1.0.0.model";
const char *face_mesh_model = "M_SenseAR_3DMesh_Face2106pt_FastV7_1.0.0.model";
const char *avatar_helper_model = "M_SenseME_Avatar_Help_2.2.0.model";
const char *avatar_model = "M_SenseME_Avatar_Core_2.0.0.model";
const char *gaze_model = "M_SenseME_GazeTracking_2.1.3.model";
const char *dynamic_gesture_model =
	"M_SenseME_Hand_Dynamic_Gesture_1.0.0.model";
const char *hand_skeleton_model = "M_SenseME_Hand_Skeleton_2d3d_1.0.0.model";
const char *multi_segment_model = "M_SenseME_Segment_Multiclass_1.0.0.model";
const char *attribute_model = "M_SenseME_Attribute_2.2.0.model";
const char *animal_model = "M_SenseME_CatFace_3.0.0.model";
const char *classify_model = "M_SenseME_Classify_3.4.10.model";
const char *verify_model = "M_SenseME_Verify_3.91.0.model";
const char *p_db_path = "M_SenseME_Classify_Table_1.0.7.db";
const char *p_custom_db_path = "M_SenseME_Classify_Custom_Table_1.0.7.db";
const char *upbody_model = "M_SenseME_Upper_Body_0.0.42.model";

static std::string g_ModelDir = "./sensetime/models/";
static std::string g_StickerDir = "./sensetime/stickers";
static std::string g_JsonDir = "./sensetime/json";
static std::string g_FilterDir = "./sensetime/filters";
static std::string g_MakeupDir = "./sensetime/makeups";

typedef enum {
	PROCESS_TEXTURE = 0,
	PROCESS_BUFFER = 1,
	PROCESS_PICTURE = 2
} st_process_method;

#include <st_mobile_human_action.h>

#ifndef DISABLE_TIMING
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#ifdef _MSC_VER
#define __TIC__() double __timing_start = clock()
#define __TOC__()                                                 \
	do {                                                      \
		double __timing_end = clock();            \
		fprintf(stdout, "TIME(ms): %lf\n",                \
			(__timing_end - __timing_start)   \
				/ CLOCKS_PER_SEC * 1000);         \
	} while (0)
static double getms() {
	return clock();
}
#else
#include <unistd.h>
#include <sys/time.h>

#define __TIC__()                                    \
	struct timeval __timing_start, __timing_end; \
	gettimeofday(&__timing_start, NULL);

#define __TOC__()                                                        \
	do {                                                             \
		gettimeofday(&__timing_end, NULL);                       \
		double __timing_gap = (__timing_end.tv_sec -     \
					       __timing_start.tv_sec) *  \
					      1000.0 +                     \
				      (__timing_end.tv_usec -    \
					       __timing_start.tv_usec) / \
					      1000.0;                    \
		fprintf(stdout, "TIME(ms): %lf\n", __timing_gap);        \
	} while (0)
static double getms() {
	struct timeval time1;
	gettimeofday(&time1, NULL);
	return time1.tv_sec* 1000.0 + time1.tv_usec / 1000.0;
}
#endif

#else
#define __TIC__()
#define __TOC__()
#endif

#ifdef _MSC_VER
#include <windows.h>
// 获取系统实际支持的指令集:
// 使用FMA/AVX参数依次运行runtime_check.exe, 若输出中包含"true", 则表示支持该指令集
static bool runtime_check(bool *fma, bool *avx) {
	if (!fma || !avx) {
		return false;
	}

	*fma = false;
	*avx = false;

	char exe[] = "sensetime/runtime_check.exe ";
	char arg[2][4] = { "FMA", "AVX" };
	PROCESS_INFORMATION pi;
	STARTUPINFOA si;
	memset(&pi, 0, sizeof(PROCESS_INFORMATION));
	memset(&si, 0, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags |= STARTF_USESTDHANDLES;	// STARTF_USESTDHANDLES is Required.
	si.dwFlags |= STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	int i;
	for (i = 0; i < 2; i++) {
		SECURITY_ATTRIBUTES saAttr;
		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
		saAttr.bInheritHandle = TRUE;
		saAttr.lpSecurityDescriptor = NULL;

		HANDLE hChildStdoutRd = NULL; // Read-side, used in calls to ReadFile() to get child's stdout output.
		HANDLE hChildStdoutWr = NULL; // Write-side, given to child process using si struct.

		// Create a pipe to get results from child's stdout.
		if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) {
			printf("cannot create pipe\n");
			return false;
		}
		// Ensure the read handle to the pipe for STDOUT is not inherited.
		if (!SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0)) {
			printf("Stdout SetHandleInformation");
			return false;
		}

		si.hStdOutput = hChildStdoutWr;		// Requires STARTF_USESTDHANDLES in dwFlags.
		si.hStdError = hChildStdoutWr;		// Requires STARTF_USESTDHANDLES in dwFlags.

		char *cmd = new char[strlen(exe) + strlen(arg[i]) + 1];
		strcpy(cmd, exe);
		strcat(cmd, arg[i]);
		BOOL bSuccess = CreateProcessA(
			NULL,	// No module name (use command line)
			cmd,	// Command line
			NULL,	// Process handle not inheritable
			NULL,	// Thread handle not inheritable
			TRUE,	// Set handle inheritance to TRUE
			0,		// No creation flags
			NULL,	// Use parent's environment block
			NULL,	// Use parent's starting directory
			&si,	// Pointer to STARTUPINFO structure
			&pi);	// Pointer to PROCESS_INFORMATION structure

		delete[] cmd;

		if (bSuccess) {
			WaitForSingleObject(pi.hProcess, INFINITE);
			// Close the write end of the pipe before reading from the read end of the pipe.
			if (!CloseHandle(hChildStdoutWr)) {
				printf("cannot close handle");
				return false;
			}

			std::string strResult;
			// Read output from the child process.
			for (;;) {
				DWORD dwRead;
				char chBuf[64];
				// Read from pipe that is the standard output for child process.
				bSuccess = ReadFile(hChildStdoutRd, chBuf, 64, &dwRead, NULL);
				if (!bSuccess || 0 == dwRead) {
					break;
				}
				strResult += std::string(chBuf, dwRead);
			}
			if (strResult.length() > 0) {
				std::size_t pos = strResult.find(":");
				strResult = strResult.substr(pos + 2, 4);
				if (strResult == "true") {
					if (0 == i) *fma = true;
					if (1 == i) *avx = true;
				}
			}

			CloseHandle(hChildStdoutRd);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
		else {
			printf("CreateProcess failed: %d\n", GetLastError());
			return false;
		}
	}

	return true;
}
#endif // _MSC_VER

#include <fstream>
#include <string>
#include <st_mobile_license.h>
#define MAX_LEN 100.0f
#define WIN(name, value, func) { \
	createTrackbar(name, wnd_name, &value, MAX_LEN, func); \
	func(value, NULL); }
#define WIN2(name, value, func) { \
	createTrackbar(name, wnd_name2, &value, MAX_LEN, func); \
	func(value, NULL); }

static int check_license_online() {
	// generate and check active code
	char activate_buf[10000] = { 0 };
	int activate_buf_len = sizeof(activate_buf);
	const char* license_path = "sensetime/license_online.lic";
	const char* activate_path = "sensetime/activate_code.lic";
	std::ifstream in(activate_path);
	if (!in.eof()) {
		in >> activate_buf;
		in.close();
	}

	int ret = st_mobile_check_activecode(license_path, activate_buf, strlen(activate_buf));
	if (ret != ST_OK) {
		printf("we will generate new activate_code %d\n", ret);
		ret = st_mobile_generate_activecode(license_path, activate_buf, &activate_buf_len);
		if (ret == ST_OK) {
			std::ofstream out(activate_path);
			out << activate_buf;
			out.close();
		}
		else {
			printf("fail to generate activate_code %d\n", ret);
			return -1;
		}
	}

	return 0;
}

int check_license() {
#ifdef _MSC_VER
	// set to SSE mode when the system dose not support FMA or AVX instruction set
	bool fma = false, avx = false;
	if (!runtime_check(&fma, &avx) || !fma || !avx) {
		st_mobile_set_sse_only(true);
	}
#endif
	return check_license_online();
}

#define MIN(a,b)  ((a) > (b) ? (b) : (a))
#define MAX(a,b)  ((a) < (b) ? (b) : (a))

typedef struct _IntPoint_ {
	int x;
	int y;
} IntPoint;

/// @brief 绘制float点，去除因取整造成的106点显示抖动
/// @param[in] image 图片地址
/// @param[in] fmt 图片格式，支持ST_PIX_FMT_GRAY8，ST_PIX_FMT_BGR888，ST_PIX_FMT_BGRA8888和ST_PIX_FMT_RGBA8888
/// @param[in] width 图片宽度
/// @param[in] height 图片高度
/// @param[in] x 绘制点的x轴坐标
/// @param[in] y 绘制点的y轴坐标
/// @param[in] radius 绘制点的半径
/// @param[in] color 绘制点的RGB值
static void draw_float_point(unsigned char* image, st_pixel_format fmt, int width, int height,
	float x, float y, float radius, unsigned int color = 0x00ff00) {
	// calculate interpolation region
	const int region_expansion = 2;
	int x_left = round(x - radius) - region_expansion;
	int x_right = round(x + radius) + region_expansion;
	int y_top = round(y - radius) - region_expansion;
	int y_bottom = round(y + radius) + region_expansion;

	// the interpolation region must be in the image
	x_left = MIN(MAX(x_left, 0), width - 1);
	x_right = MIN(MAX(x_right, 0), width - 1);
	y_top = MIN(MAX(y_top, 0), height - 1);
	y_bottom = MIN(MAX(y_bottom, 0), height - 1);

	const int num_region_points = (x_right - x_left + 1) * (y_bottom - y_top + 1);
	IntPoint *points = (IntPoint*)malloc(sizeof(IntPoint) * num_region_points);
	float *ratios = (float*)malloc(sizeof(float) * num_region_points);

	int num_points = 0;
	for (int i = x_left; i <= x_right; i++) {
		for (int j = y_top; j <= y_bottom; j++) {
			float dx = i - x;
			float dy = j - y;
			float dis = sqrt(dx * dx + dy * dy);
			if (dis < radius + 0.5f) {
				points[num_points].x = i;
				points[num_points].y = j;
				ratios[num_points] = MIN(radius + 0.5f - dis, 1.0f);
				num_points++;
			}
		}
	}

	int pixel_size = 1;
	switch (fmt) {
	case ST_PIX_FMT_GRAY8:
		pixel_size = 1;
		break;
	case ST_PIX_FMT_BGR888:
		pixel_size = 3;
		break;
	case ST_PIX_FMT_BGRA8888:
		pixel_size = 4;
		break;
	default: break;
	}

	unsigned char *color_byte = (unsigned char *)(&color);
	for (int i = 0; i < num_points; i++) {
		unsigned char* pixel = image + (points[i].x + points[i].y * width) * pixel_size;
		if (1 == pixel_size) {
			pixel[0] = 0xff * ratios[i] + pixel[0] * (1.0f - ratios[i]);
		}
		else {
			pixel[0] = color_byte[0] * ratios[i] + pixel[0] * (1.0f - ratios[i]);
			pixel[1] = color_byte[1] * ratios[i] + pixel[1] * (1.0f - ratios[i]);
			pixel[2] = color_byte[2] * ratios[i] + pixel[2] * (1.0f - ratios[i]);
		}
	}

	free(points);
	free(ratios);
}
#include <string>
#include <vector>
#ifdef _MSC_VER
#include<io.h>
#include <direct.h> 
#else
#include<unistd.h>  
#include <sys/types.h>  
#include <sys/stat.h> 
#include <dirent.h>
#endif

inline void DfsFolder(const std::string& DirPath, const std::string& suffix, std::vector<std::string>&  files)
{
#ifdef _MSC_VER
	_finddata_t file_info;
	std::string current_path = DirPath + "/*.*";
	intptr_t handle = _findfirst(current_path.c_str(), &file_info);
	if (-1 == handle) {
		return;
	}

	do {
		std::string tmpName = file_info.name;
		if (tmpName.find(".") == 0)
			continue;
		std::string FullPath = DirPath + "/" + tmpName;
		if (file_info.attrib == _A_SUBDIR) {
			DfsFolder(FullPath, suffix, files);
		}
		else if (tmpName.find(suffix) != std::string::npos) {
			files.push_back(FullPath);
		}
	} while (!_findnext(handle, &file_info));
	_findclose(handle);
#else
	DIR *d = opendir(DirPath.c_str());
	if (!d) {
		printf("cannot match the path ??? %s", DirPath.c_str());
		return;
	}

	struct dirent *file;
	struct stat sb;
	while ((file = readdir(d)) != NULL)
	{
		std::string tmpName = file->d_name;
		if (tmpName.find(".") == 0)
			continue;
		std::string FullPath = DirPath + "/" + tmpName;
		if (stat(FullPath.c_str(), &sb) >= 0 && S_ISDIR(sb.st_mode))
		{
			DfsFolder(FullPath, suffix, files);
		}
		else if (tmpName.find(suffix) != std::string::npos) {
			files.push_back(FullPath);
		}
	}
	closedir(d);
#endif
}

#define ParseKey(x) if ((char)x == '+') { \
	capture.set(CV_CAP_PROP_FRAME_WIDTH, 1280); \
	capture.set(CV_CAP_PROP_FRAME_HEIGHT, 720); \
} else if ((char)x == '-') { \
	capture.set(CV_CAP_PROP_FRAME_WIDTH, 640); \
	capture.set(CV_CAP_PROP_FRAME_HEIGHT, 480); \
	} else if (x == 27 || x == 'q') break;

#endif // ST_MOBILE_INCLUDE_HELPER_H_
