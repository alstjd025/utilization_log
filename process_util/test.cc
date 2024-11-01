#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <dirent.h>
#include <unistd.h>
#include <vector>
#include <chrono>
#include <thread>
#include <sys/sysinfo.h>

using namespace std;

struct ProcessInfo {
    int pid;
    unsigned long utime;
    unsigned long stime;
};

int getCurrentPid() {
    return getpid();
}

bool getProcessCpuUsage(int pid, ProcessInfo &info) {
    stringstream ss;
    ss << "/proc/" << pid << "/stat";
    ifstream statFile(ss.str());

    if (!statFile.is_open()) {
        return false;
    }

    string line;
    getline(statFile, line);
    statFile.close();

    stringstream lineStream(line);
    lineStream >> info.pid;

    // comm 필드 (프로세스 이름)을 건너뜀
    string comm;
    lineStream >> comm;

    // state 필드를 건너뜀
    char state;
    lineStream >> state;

    // utime은 10번째 필드, stime은 11번째 필드
    for (int i = 0; i < 10; ++i) {
        lineStream >> comm; // 임시로 사용
    }
    lineStream >> info.utime >> info.stime;

    return true;
}

vector<int> getAllPids() {
    vector<int> pids;
    DIR *dir = opendir("/proc");
    if (dir == nullptr) {
        return pids;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (isdigit(*entry->d_name)) {
            pids.push_back(atoi(entry->d_name));
        }
    }

    closedir(dir);
    return pids;
}

unsigned long getTotalCpuTime() {
    ifstream statFile("/proc/stat");
    if (!statFile.is_open()) {
        return 0;
    }

    string line;
    getline(statFile, line);
    statFile.close();

    string dummy;
    unsigned long user, nice, system, idle, iowait, irq, softirq;
    stringstream lineStream(line);
    lineStream >> dummy >> user >> nice >> system >> idle >> iowait >> irq >> softirq;

    return user + nice + system + idle + iowait + irq + softirq;
}

int main() {
    int excludePid;
    cout << "Enter the PID to exclude: ";
    cin >> excludePid;

    // 시스템의 코어 수 가져오기
    int numCores = get_nprocs();

    while (true) {
        vector<int> pids = getAllPids();

        // 첫 번째 측정 시점
        unsigned long totalCpuTime1 = getTotalCpuTime();
        vector<ProcessInfo> processInfoList1;

        for (int pid : pids) {
            if (pid == excludePid) {
                continue;
            }

            ProcessInfo info;
            if (getProcessCpuUsage(pid, info)) {
                processInfoList1.push_back(info);
            }
        }

        // 일정 시간 대기 (10ms)
        this_thread::sleep_for(chrono::milliseconds(10));

        // 두 번째 측정 시점
        unsigned long totalCpuTime2 = getTotalCpuTime();
        vector<ProcessInfo> processInfoList2;

        for (int pid : pids) {
            if (pid == excludePid) {
                continue;
            }

            ProcessInfo info;
            if (getProcessCpuUsage(pid, info)) {
                processInfoList2.push_back(info);
            }
        }

        // CPU 사용량 계산
        unsigned long totalCpuDelta = totalCpuTime2 - totalCpuTime1;
        unsigned long totalProcessCpuDelta = 0;

        for (size_t i = 0; i < processInfoList1.size(); ++i) {
            unsigned long utimeDelta = processInfoList2[i].utime - processInfoList1[i].utime;
            unsigned long stimeDelta = processInfoList2[i].stime - processInfoList1[i].stime;
            totalProcessCpuDelta += (utimeDelta + stimeDelta);
        }

        double cpuUsagePercent = (totalProcessCpuDelta / static_cast<double>(totalCpuDelta)) * 100.0 * numCores;

        cout << "Total CPU Usage (excluding PID " << excludePid << "): " << cpuUsagePercent << "%" << endl;

        // 일정 시간 대기 (10ms) 후 다시 측정
        this_thread::sleep_for(chrono::milliseconds(10));
    }

    return 0;
}
