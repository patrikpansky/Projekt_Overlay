#include <iostream>
#include <fstream>
#include <sqlite3.h>
#include <string>
#include <iomanip>
#include <chrono>
#include <thread>
#include <windows.h>

std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &time);
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buffer);
}
double getRAMUsage() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (!GlobalMemoryStatusEx(&memInfo)) {
        return -1.0;
    }
    return 100.0 - (double(memInfo.ullAvailPhys) / memInfo.ullTotalPhys) * 100.0;
}
double getCPUUsage() {
    static ULARGE_INTEGER prevIdleTime, prevKernelTime, prevUserTime;

    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime) == 0) {
        return -1.0;
    }

    ULARGE_INTEGER idle, kernel, user;
    idle.LowPart = idleTime.dwLowDateTime;
    idle.HighPart = idleTime.dwHighDateTime;
    kernel.LowPart = kernelTime.dwLowDateTime;
    kernel.HighPart = kernelTime.dwHighDateTime;
    user.LowPart = userTime.dwLowDateTime;
    user.HighPart = userTime.dwHighDateTime;

    ULONGLONG idleDiff = idle.QuadPart - prevIdleTime.QuadPart;
    ULONGLONG kernelDiff = kernel.QuadPart - prevKernelTime.QuadPart;
    ULONGLONG userDiff = user.QuadPart - prevUserTime.QuadPart;

    prevIdleTime = idle;
    prevKernelTime = kernel;
    prevUserTime = user;

    ULONGLONG totalDiff = kernelDiff + userDiff;
    return totalDiff > 0 ? (1.0 - (double)idleDiff / totalDiff) * 100.0 : 0.0;
}

void saveToDatabase(const std::string& dbName, const std::string& time, double cpu, double ram) {
    sqlite3* db;
    char* errMsg = nullptr;

    if (sqlite3_open(dbName.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    std::string createTableSQL =
        "CREATE TABLE IF NOT EXISTS Performance ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "timestamp TEXT, "
        "cpu_usage REAL, "
        "ram_usage REAL);";

    if (sqlite3_exec(db, createTableSQL.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Error creating table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return;
    }
    std::string insertSQL =
        "INSERT INTO Performance (timestamp, cpu_usage, ram_usage) "
        "VALUES ('" + time + "', " + std::to_string(cpu) + ", " + std::to_string(ram) + ");";

    if (sqlite3_exec(db, insertSQL.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Error inserting data: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
    sqlite3_close(db);
}

int main() {
    const std::string dbName = "performance_data.db";
    for (int i = 0; i < 20; ++i) {                          // Iterace
        std::string time = getCurrentTime();
        double cpu = getCPUUsage();
        double ram = getRAMUsage();

        std::cout << "Time: " << time
            << " | CPU Usage: " << std::fixed << std::setprecision(2) << cpu
            << "% | RAM Usage: " << ram << "%" << std::endl;

        saveToDatabase(dbName, time, cpu, ram);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "Data saved to database: " << dbName << std::endl;
    return 0;
}