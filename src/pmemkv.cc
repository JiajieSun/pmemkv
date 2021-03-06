/*
 * Copyright 2017-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "engines/blackhole.h"
#include "engines/kvtree2.h"
#include "engines/btree.h"
#include "engines/mvtree.h"

namespace pmemkv {

KVEngine* KVEngine::Open(const string& engine,
                         const string& path,
                         const size_t size,
                         const string& layout
                         ) {
    try {
        if (engine == blackhole::ENGINE) {
            return new blackhole::Blackhole();
        } else if(engine == mvtree::ENGINE) {
            return new mvtree::MVTree(path, size, layout);
        }  else if (engine == kvtree2::ENGINE) {
            return new kvtree2::KVTree(path, size, layout);
        } else if (engine == btree::ENGINE) {
            return new btree::BTreeEngine(path, size, layout);
        } else {
            return nullptr;
        }
    } catch (...) {
        return nullptr;
    }
}

KVEngine* KVEngine::Open(const string& engine,           
                         const string& path,           
                         size_t size) {
    return Open(engine, path, size, LAYOUT);
}



KVEngine* KVEngine::Open(const string& engine, PMEMobjpool* pop) {
     try {
        if(engine == mvtree::ENGINE) {
            return new mvtree::MVTree(pop);
        } else {
            return nullptr;
        }
    } catch (...) {
        return nullptr;
    } 
}

KVEngine* KVEngine::Open(const string& engine, PMEMobjpool* pop, const PMEMoid& oid) {
    try {
        if(engine == mvtree::ENGINE) {
            return new mvtree::MVTree(pop, oid);
        } else {
            return nullptr;
        }
    } catch (...) {
        return nullptr;
    }
}


void KVEngine::Close(KVEngine* kv) {
    auto engine = kv->Engine();
    if (engine == blackhole::ENGINE) {
        delete (blackhole::Blackhole*) kv;
    } else if (engine == mvtree::ENGINE) {
        delete (mvtree::MVTree*) kv;
    } else if (engine == kvtree2::ENGINE) {
        delete (kvtree2::KVTree*) kv;
    } else if (engine == btree::ENGINE) {
        delete (btree::BTreeEngine*) kv;
    }
    kv = nullptr;
}

void KVEngine::Free(KVEngine* kv) {
    auto engine = kv->Engine();
    kv->Free();
    // TODO free and close shall be transactional?
    KVEngine::Close(kv);
}

extern "C" KVEngine* kvengine_open(const char* engine, const char* path, const size_t size) {
    return KVEngine::Open(engine, path, size);
};

extern "C" KVEngine* kvengine_open_with_layout(const char* engine, const char* path, const size_t size, const char* layout) {
    return KVEngine::Open(engine, path, size, layout);
};
   
extern "C" KVEngine* kvengine_open_root(const char* engine, PMEMobjpool* pop) {
    return KVEngine::Open(engine, pop);
};
   
extern "C" KVEngine* kvengine_open_obj(const char* engine, PMEMobjpool* pop, PMEMoid oid) {
    return KVEngine::Open(engine, pop, oid);
}

extern "C" void kvengine_close(KVEngine* kv) {
    return KVEngine::Close(kv);
};
extern "C" void kvengine_free(KVEngine* kv) {
    return KVEngine::Free(kv);
};

extern "C" int8_t kvengine_get(KVEngine* kv, const int32_t limit, const int32_t keybytes,
                               int32_t* valuebytes, const char* key, char* value) {
    return kv->Get(limit, keybytes, valuebytes, key, value);
}

extern "C" int8_t kvengine_put(KVEngine* kv, const int32_t keybytes, int32_t* valuebytes,
                               const char* key, const char* value) {
    return kv->Put(string(key, (size_t) keybytes), string(value, (size_t) *valuebytes));
}

extern "C" int8_t kvengine_remove(KVEngine* kv, const int32_t keybytes, const char* key) {
    return kv->Remove(string(key, (size_t) keybytes));
};

extern "C" int8_t kvengine_get_ffi(FFIBuffer* buf) {
    return buf->kv->Get(buf->limit, buf->keybytes, &buf->valuebytes,
                        buf->data, buf->data + buf->keybytes);
}

extern "C" int8_t kvengine_put_ffi(const FFIBuffer* buf) {
    return buf->kv->Put(string(buf->data, (size_t) buf->keybytes),
                        string(buf->data + buf->keybytes, (size_t) buf->valuebytes));
}

extern "C" int8_t kvengine_remove_ffi(const FFIBuffer* buf) {
    return buf->kv->Remove(string(buf->data, (size_t) buf->keybytes));
}

extern "C" PMEMoid kvengine_get_rootoid(KVEngine* kv) {
    return kv->GetRootOid();
}
extern "C" PMEMobjpool* kvengine_get_pool(KVEngine* kv) {
    return kv->GetPool();
}


// todo missing test cases for KVEngine static methods & extern C API

} // namespace pmemkv
