#include <Windows.h>
#include <Psapi.h>
#include <TlHelp32.h>
#include <util/dstr.h>
#include <obs.h>
#include <map>
#include <list>
#include <vector>
#include <set>
#include <sstream>

extern "C" {
extern int s_cmp(const char *str1, const char *str2);
extern HANDLE open_process(DWORD desired_access, bool inherit_handle, DWORD process_id);
bool get_pid_exe(struct dstr *name, DWORD id)
{
	wchar_t wname[MAX_PATH];
	struct dstr temp = {0};
	bool success = false;
	HANDLE process = NULL;
	char *slash;

	process = open_process(PROCESS_QUERY_LIMITED_INFORMATION, false, id);
	if (!process)
		goto fail;

	if (!GetProcessImageFileNameW(process, wname, MAX_PATH))
		goto fail;

	dstr_from_wcs(&temp, wname);
	if (strstr(temp.array, "\\Windows\\System32") != NULL || strstr(temp.array, "Microsoft Visual Studio") != NULL)
		goto fail;

	slash = strrchr(temp.array, '\\');
	if (!slash)
		goto fail;

	dstr_copy(name, slash + 1);
	success = true;

fail:
	if (!success)
		dstr_copy(name, "unknown");

	dstr_free(&temp);
	CloseHandle(process);
	return true;
}

struct ProcessInfo {
	DWORD id;
	DWORD parent_id;
};

bool find_selectd_process(const char *process_image_name, DWORD *id)
{
	//exe:pid
	if (!process_image_name || strlen(process_image_name) == 0)
		return false;

	std::vector<ProcessInfo> processes;

	PROCESSENTRY32 pe32;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap != INVALID_HANDLE_VALUE) {
		pe32.dwSize = sizeof(PROCESSENTRY32);
		if (Process32First(hSnap, &pe32)) {
			do {
				if (pe32.th32ProcessID == 0)
					continue;

				struct dstr exe_name = {0};
				dstr_from_wcs(&exe_name, pe32.szExeFile);
				if (strcmp(process_image_name, exe_name.array) == 0) {
					processes.push_back({pe32.th32ProcessID, pe32.th32ParentProcessID});
				}
				dstr_free(&exe_name);
			} while (Process32Next(hSnap, &pe32));
		}
		CloseHandle(hSnap);
	}

	if (processes.size() == 0)
		return false;

	if (processes.size() == 1) {
		*id = processes[0].id;
		return true;
	}

	std::set<DWORD> ids;
	std::set<DWORD> parent_ids;
	for (size_t i = 0; i < processes.size(); i++) {
		ids.insert(processes[i].id);
		parent_ids.insert(processes[i].parent_id);
	}

	for (auto iter = ids.begin(); iter != ids.end(); iter++) {
		if (parent_ids.count(*iter) > 0) {
			*id = *iter;
			return true;
		}
	}

	return false;
}

void fill_process_list(obs_property_t *p)
{
	DWORD processes[1024], process_count;
	unsigned int i;

	if (!EnumProcesses(processes, sizeof(processes), &process_count))
		return;

	std::map<std::string, std::list<DWORD>> all_process;
	process_count = process_count / sizeof(DWORD);
	for (i = 0; i < process_count; i++) {
		if (processes[i] != 0) {
			struct dstr exe = {0};
			get_pid_exe(&exe, processes[i]);
			if (s_cmp(exe.array, "unknown") != 0) {
				std::list<DWORD> &one = all_process[std::string(exe.array)];
				one.push_back(processes[i]);
			}
			dstr_free(&exe);
		}
	}

	for (auto iter = all_process.begin(); iter != all_process.end(); iter++) {
		std::list<DWORD> &one = iter->second;
		for (auto iter2 = one.begin(); iter2 != one.end(); iter2++) {
			char buf[MAX_PATH] = {0};
			sprintf(buf, "%s:%d", iter->first.c_str(), *iter2);
			obs_property_list_add_string(p, buf, buf);
		}
	}
}
}
