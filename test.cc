
#include "ExeLoader.h"

#include <stdio.h>

using namespace NS_ExeLoader;

int main()
{
    using addr_t = ExeLoader::addr_t;
    using size_t = ExeLoader::size_t;
    using prop_t = ExeLoader::prop_t;

    auto *loader = createExeLoader("bench.elf");

    ExeLoader::CallBack cb;
    cb.loadData = [](addr_t va, void *p, size_t size, prop_t prop) {
        printf("load va=%lx ptr=%p size=%d prop=%x\n", va, p, size, prop);
    };
    cb.setZero = [](addr_t va, size_t size, prop_t prop) {
        printf("zero va=%lx size=%d prop=%x\n", va, size, prop);
    };

    loader->setCallBack(cb);
    loader->loadFile();

    auto va = loader->findSymbol("main");
    printf("main: %lx\n", va);
}

