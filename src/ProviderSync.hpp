#pragma once
#include <string>
#include <sqlite3.h>

class ProviderSync {
public:
    static void sync_models_from_provider(sqlite3* db, const std::string& provider, const std::string& api_key);
};
