// -*- C++ -*-
// public header

#pragma once

#include <cstdint>
#include <string>
#include <functional>

#ifndef NS_ExeLoader
#define NS_ExeLoader exe_loader
#endif

namespace NS_ExeLoader {

using std::string;

class ExeLoader {
public:
    typedef std::uint64_t addr_t;
    typedef std::uint32_t size_t;
    typedef std::uint32_t prop_t;

    enum Property {
        Read, Write, Execute
    };

    struct CallBack {
        std::function<void(addr_t, void*, size_t, prop_t)> loadData;
        std::function<void(addr_t, size_t, prop_t)>        setZero;

        // call as setReg(reg_name, &reg_value, sizeof(reg_value));
        std::function<void(const string &, void*, size_t)> setReg;
    };

    virtual void setCallBack(const CallBack &cb);

    /* you can change CallBack and call loadFile() again. */
    virtual void loadFile() = 0;

    virtual addr_t findSymbol(const string &name) = 0;

    ExeLoader(const std::string &path) : path_(path) { }

    template <class T>
    static ExeLoader * create(const std::string &path);

protected:
    std::string path_;
    CallBack cb_;
    addr_t entry_point_;
    addr_t vm_start_, vm_end_;  // memory region used
};

// create an executable load which math the file format
ExeLoader *createExeLoader(const char *path);

} // namespace

