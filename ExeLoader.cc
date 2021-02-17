
#include "ExeLoader.h"


namespace NS_ExeLoader {

void ExeLoader::setCallBack(const CallBack &cb) {
    cb_ = cb;
}



class ElfLoader;
class PteLoader;
class ArmChkptLoader;

ExeLoader *createExeLoader(const char *path) {
    ExeLoader *loader = nullptr;
    loader = ExeLoader::create<ElfLoader>(path);
    //if (not loader)
    //    ExeLoader::create<PteLoader>(path);
    return loader;
}

} // namespace

