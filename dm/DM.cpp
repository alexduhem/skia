#include "CrashHandler.h"
#include "DMJsonWriter.h"
#include "DMSrcSink.h"
#include "OverwriteLine.h"
#include "ProcStats.h"
#include "SkBBHFactory.h"
#include "SkChecksum.h"
#include "SkCommonFlags.h"
#include "SkForceLinking.h"
#include "SkGraphics.h"
#include "SkInstCnt.h"
#include "SkMD5.h"
#include "SkOSFile.h"
#include "SkTDynamicHash.h"
#include "SkTaskGroup.h"
#include "Test.h"
#include "Timer.h"

DEFINE_string(images, "resources", "Images to decode.");
DEFINE_string(src, "tests gm skp image subset", "Source types to test.");
DEFINE_bool(nameByHash, false,
            "If true, write to FLAGS_writePath[0]/<hash>.png instead of "
            "to FLAGS_writePath[0]/<config>/<sourceType>/<name>.png");
DEFINE_bool2(pathOpsExtended, x, false, "Run extended pathOps tests.");
DEFINE_string(matrix, "1 0 0 0 1 0 0 0 1",
              "Matrix to apply when using 'matrix' in config.");
DEFINE_bool(gpu_threading, false, "Allow GPU work to run on multiple threads?");

DEFINE_string(blacklist, "",
        "Space-separated config/src/name triples to blacklist.  '_' matches anything.  E.g. \n"
        "'--blacklist gpu skp _' will blacklist all SKPs drawn into the gpu config.\n"
        "'--blacklist gpu skp _ 8888 gm aarects' will also blacklist the aarects GM on 8888.");

DEFINE_string2(readPath, r, "", "If set check for equality with golden results in this directory.");

__SK_FORCE_IMAGE_DECODER_LINKING;
using namespace DM;

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

SK_DECLARE_STATIC_MUTEX(gFailuresMutex);
static SkTArray<SkString> gFailures;

static void fail(ImplicitString err) {
    SkAutoMutexAcquire lock(gFailuresMutex);
    SkDebugf("\n\nFAILURE: %s\n\n", err.c_str());
    gFailures.push_back(err);
}

static int32_t gPending = 0;  // Atomic.

static void done(double ms,
                 ImplicitString config, ImplicitString src, ImplicitString name,
                 ImplicitString log) {
    if (!log.isEmpty()) {
        log.prepend("\n");
    }
    auto pending = sk_atomic_dec(&gPending)-1;
    SkDebugf("%s(%4dMB %5d) %s\t%s %s %s%s", FLAGS_verbose ? "\n" : kSkOverwriteLine
                                           , sk_tools::getMaxResidentSetSizeMB()
                                           , pending
                                           , HumanizeMs(ms).c_str()
                                           , config.c_str()
                                           , src.c_str()
                                           , name.c_str()
                                           , log.c_str());
    // We write our dm.json file every once in a while in case we crash.
    // Notice this also handles the final dm.json when pending == 0.
    if (pending % 500 == 0) {
        JsonWriter::DumpJson();
    }
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

struct Gold : public SkString {
    Gold(ImplicitString sink, ImplicitString src, ImplicitString name, ImplicitString md5)
        : SkString("") {
        this->append(sink);
        this->append(src);
        this->append(name);
        this->append(md5);
        while (this->size() % 4) {
            this->append("!");  // Pad out if needed so we can pass this to Murmur3.
        }
    }
    static const Gold& GetKey(const Gold& g) { return g; }
    static uint32_t Hash(const Gold& g) {
        return SkChecksum::Murmur3((const uint32_t*)g.c_str(), g.size());
    }
};
static SkTDynamicHash<Gold, Gold> gGold;

static void add_gold(JsonWriter::BitmapResult r) {
    gGold.add(new Gold(r.config, r.sourceType, r.name, r.md5));  // We'll let these leak. Lazybones.
}

static void gather_gold() {
    if (!FLAGS_readPath.isEmpty()) {
        SkString path(FLAGS_readPath[0]);
        path.append("/dm.json");
        if (!JsonWriter::ReadJson(path.c_str(), add_gold)) {
            fail(SkStringPrintf("Couldn't read %s for golden results.", path.c_str()));
        }
    }
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

template <typename T>
struct Tagged : public SkAutoTDelete<T> { const char* tag; };

static const bool kMemcpyOK = true;

static SkTArray<Tagged<Src>,  kMemcpyOK>  gSrcs;
static SkTArray<Tagged<Sink>, kMemcpyOK> gSinks;

static void push_src(const char* tag, Src* s) {
    SkAutoTDelete<Src> src(s);
    if (FLAGS_src.contains(tag) &&
        !SkCommandLineFlags::ShouldSkip(FLAGS_match, src->name().c_str())) {
        Tagged<Src>& s = gSrcs.push_back();
        s.reset(src.detach());
        s.tag = tag;
    }
}

static void gather_srcs() {
    for (const skiagm::GMRegistry* r = skiagm::GMRegistry::Head(); r; r = r->next()) {
        push_src("gm", new GMSrc(r->factory()));
    }
    for (int i = 0; i < FLAGS_skps.count(); i++) {
        const char* path = FLAGS_skps[i];
        if (sk_isdir(path)) {
            SkOSFile::Iter it(path, "skp");
            for (SkString file; it.next(&file); ) {
                push_src("skp", new SKPSrc(SkOSPath::Join(path, file.c_str())));
            }
        } else {
            push_src("skp", new SKPSrc(path));
        }
    }
    static const char* const exts[] = {
        "bmp", "gif", "jpg", "jpeg", "png", "webp", "ktx", "astc", "wbmp", "ico",
        "BMP", "GIF", "JPG", "JPEG", "PNG", "WEBP", "KTX", "ASTC", "WBMP", "ICO",
    };
    for (int i = 0; i < FLAGS_images.count(); i++) {
        const char* flag = FLAGS_images[i];
        if (sk_isdir(flag)) {
            for (size_t j = 0; j < SK_ARRAY_COUNT(exts); j++) {
                SkOSFile::Iter it(flag, exts[j]);
                for (SkString file; it.next(&file); ) {
                    SkString path = SkOSPath::Join(flag, file.c_str());
                    push_src("image", new ImageSrc(path));     // Decode entire image.
                    push_src("subset", new ImageSrc(path, 2)); // Decode into 2 x 2 subsets
                }
            }
        } else if (sk_exists(flag)) {
            // assume that FLAGS_images[i] is a valid image if it is a file.
            push_src("image", new ImageSrc(flag));     // Decode entire image.
            push_src("subset", new ImageSrc(flag, 2)); // Decode into 2 x 2 subsets
        }
    }
}

static GrGLStandard get_gpu_api() {
    if (FLAGS_gpuAPI.contains("gl"))   { return kGL_GrGLStandard; }
    if (FLAGS_gpuAPI.contains("gles")) { return kGLES_GrGLStandard; }
    return kNone_GrGLStandard;
}

static void push_sink(const char* tag, Sink* s) {
    SkAutoTDelete<Sink> sink(s);
    if (!FLAGS_config.contains(tag)) {
        return;
    }
    // Try a noop Src as a canary.  If it fails, skip this sink.
    struct : public Src {
        Error draw(SkCanvas*) const SK_OVERRIDE { return ""; }
        SkISize size() const SK_OVERRIDE { return SkISize::Make(16, 16); }
        Name name() const SK_OVERRIDE { return "noop"; }
    } noop;

    SkBitmap bitmap;
    SkDynamicMemoryWStream stream;
    SkString log;
    Error err = sink->draw(noop, &bitmap, &stream, &log);
    if (!err.isEmpty()) {
        SkDebugf("Skipping %s: %s\n", tag, err.c_str());
        return;
    }

    Tagged<Sink>& ts = gSinks.push_back();
    ts.reset(sink.detach());
    ts.tag = tag;
}

static bool gpu_supported() {
#if SK_SUPPORT_GPU
    return FLAGS_gpu;
#else
    return false;
#endif
}

static Sink* create_sink(const char* tag) {
#define SINK(t, sink, ...) if (0 == strcmp(t, tag)) { return new sink(__VA_ARGS__); }
    if (gpu_supported()) {
        typedef GrContextFactory Gr;
        const GrGLStandard api = get_gpu_api();
        SINK("gpunull",    GPUSink, Gr::kNull_GLContextType,   api,  0, false, FLAGS_gpu_threading);
        SINK("gpudebug",   GPUSink, Gr::kDebug_GLContextType,  api,  0, false, FLAGS_gpu_threading);
        SINK("gpu",        GPUSink, Gr::kNative_GLContextType, api,  0, false, FLAGS_gpu_threading);
        SINK("gpudft",     GPUSink, Gr::kNative_GLContextType, api,  0,  true, FLAGS_gpu_threading);
        SINK("msaa4",      GPUSink, Gr::kNative_GLContextType, api,  4, false, FLAGS_gpu_threading);
        SINK("msaa16",     GPUSink, Gr::kNative_GLContextType, api, 16, false, FLAGS_gpu_threading);
        SINK("nvprmsaa4",  GPUSink, Gr::kNVPR_GLContextType,   api,  4, false, FLAGS_gpu_threading);
        SINK("nvprmsaa16", GPUSink, Gr::kNVPR_GLContextType,   api, 16, false, FLAGS_gpu_threading);
    #if SK_ANGLE
        SINK("angle",      GPUSink, Gr::kANGLE_GLContextType,  api,  0, false, FLAGS_gpu_threading);
    #endif
    #if SK_MESA
        SINK("mesa",       GPUSink, Gr::kMESA_GLContextType,   api,  0, false, FLAGS_gpu_threading);
    #endif
    }

    if (FLAGS_cpu) {
        SINK("565",  RasterSink, kRGB_565_SkColorType);
        SINK("8888", RasterSink, kN32_SkColorType);
        SINK("pdf",  PDFSink);
        SINK("skp",  SKPSink);
        SINK("svg",  SVGSink);
        SINK("null", NullSink);
    }
#undef SINK
    return NULL;
}

static Sink* create_via(const char* tag, Sink* wrapped) {
#define VIA(t, via, ...) if (0 == strcmp(t, tag)) { return new via(__VA_ARGS__); }
    VIA("pipe",      ViaPipe,          wrapped);
    VIA("serialize", ViaSerialization, wrapped);
    VIA("tiles",     ViaTiles, 256, 256,               NULL, wrapped);
    VIA("tiles_rt",  ViaTiles, 256, 256, new SkRTreeFactory, wrapped);

    if (FLAGS_matrix.count() == 9) {
        SkMatrix m;
        for (int i = 0; i < 9; i++) {
            m[i] = (SkScalar)atof(FLAGS_matrix[i]);
        }
        VIA("matrix", ViaMatrix, m, wrapped);
    }
#undef VIA
    return NULL;
}

static void gather_sinks() {
    for (int i = 0; i < FLAGS_config.count(); i++) {
        const char* config = FLAGS_config[i];
        SkTArray<SkString> parts;
        SkStrSplit(config, "-", &parts);

        Sink* sink = NULL;
        for (int i = parts.count(); i-- > 0;) {
            const char* part = parts[i].c_str();
            Sink* next = (sink == NULL) ? create_sink(part) : create_via(part, sink);
            if (next == NULL) {
                SkDebugf("Skipping %s: Don't understand '%s'.\n", config, part);
                delete sink;
                sink = NULL;
                break;
            }
            sink = next;
        }
        if (sink) {
            push_sink(config, sink);
        }
    }
}

static bool match(const char* needle, const char* haystack) {
    return 0 == strcmp("_", needle) || NULL != strstr(haystack, needle);
}

static ImplicitString is_blacklisted(const char* sink, const char* src, const char* name) {
    for (int i = 0; i < FLAGS_blacklist.count() - 2; i += 3) {
        if (match(FLAGS_blacklist[i+0], sink) &&
            match(FLAGS_blacklist[i+1],  src) &&
            match(FLAGS_blacklist[i+2], name)) {
            return SkStringPrintf("%s %s %s",
                                  FLAGS_blacklist[i+0], FLAGS_blacklist[i+1], FLAGS_blacklist[i+2]);
        }
    }
    return "";
}

// The finest-grained unit of work we can run: draw a single Src into a single Sink,
// report any errors, and perhaps write out the output: a .png of the bitmap, or a raw stream.
struct Task {
    Task(const Tagged<Src>& src, const Tagged<Sink>& sink) : src(src), sink(sink) {}
    const Tagged<Src>&  src;
    const Tagged<Sink>& sink;

    static void Run(Task* task) {
        SkString name = task->src->name();
        SkString whyBlacklisted = is_blacklisted(task->sink.tag, task->src.tag, name.c_str());
        SkString log;
        WallTimer timer;
        timer.start();
        if (!FLAGS_dryRun && whyBlacklisted.isEmpty()) {
            SkBitmap bitmap;
            SkDynamicMemoryWStream stream;
            Error err = task->sink->draw(*task->src, &bitmap, &stream, &log);
            if (!err.isEmpty()) {
                fail(SkStringPrintf("%s %s %s: %s",
                                    task->sink.tag,
                                    task->src.tag,
                                    name.c_str(),
                                    err.c_str()));
            }
            SkAutoTDelete<SkStreamAsset> data(stream.detachAsStream());

            SkString md5;
            if (!FLAGS_writePath.isEmpty() || !FLAGS_readPath.isEmpty()) {
                SkMD5 hash;
                if (data->getLength()) {
                    hash.writeStream(data, data->getLength());
                    data->rewind();
                } else {
                    hash.write(bitmap.getPixels(), bitmap.getSize());
                }
                SkMD5::Digest digest;
                hash.finish(digest);
                for (int i = 0; i < 16; i++) {
                    md5.appendf("%02x", digest.data[i]);
                }
            }

            if (!FLAGS_readPath.isEmpty() &&
                !gGold.find(Gold(task->sink.tag, task->src.tag, name, md5))) {
                fail(SkStringPrintf("%s not found for %s %s %s in %s",
                                    md5.c_str(),
                                    task->sink.tag,
                                    task->src.tag,
                                    name.c_str(),
                                    FLAGS_readPath[0]));
            }

            if (!FLAGS_writePath.isEmpty()) {
                const char* ext = task->sink->fileExtension();
                if (data->getLength()) {
                    WriteToDisk(*task, md5, ext, data, data->getLength(), NULL);
                    SkASSERT(bitmap.drawsNothing());
                } else if (!bitmap.drawsNothing()) {
                    WriteToDisk(*task, md5, ext, NULL, 0, &bitmap);
                }
            }
        }
        timer.end();
        if (!whyBlacklisted.isEmpty()) {
            name.appendf(" (--blacklist, %s)", whyBlacklisted.c_str());
        }
        done(timer.fWall, task->sink.tag, task->src.tag, name, log);
    }

    static void WriteToDisk(const Task& task,
                            SkString md5,
                            const char* ext,
                            SkStream* data, size_t len,
                            const SkBitmap* bitmap) {
        JsonWriter::BitmapResult result;
        result.name       = task.src->name();
        result.config     = task.sink.tag;
        result.sourceType = task.src.tag;
        result.ext        = ext;
        result.md5        = md5;
        JsonWriter::AddBitmapResult(result);

        const char* dir = FLAGS_writePath[0];
        if (0 == strcmp(dir, "@")) {  // Needed for iOS.
            dir = FLAGS_resourcePath[0];
        }
        sk_mkdir(dir);

        SkString path;
        if (FLAGS_nameByHash) {
            path = SkOSPath::Join(dir, result.md5.c_str());
            path.append(".");
            path.append(ext);
            if (sk_exists(path.c_str())) {
                return;  // Content-addressed.  If it exists already, we're done.
            }
        } else {
            path = SkOSPath::Join(dir, task.sink.tag);
            sk_mkdir(path.c_str());
            path = SkOSPath::Join(path.c_str(), task.src.tag);
            sk_mkdir(path.c_str());
            path = SkOSPath::Join(path.c_str(), task.src->name().c_str());
            path.append(".");
            path.append(ext);
        }

        SkFILEWStream file(path.c_str());
        if (!file.isValid()) {
            fail(SkStringPrintf("Can't open %s for writing.\n", path.c_str()));
            return;
        }

        if (bitmap) {
            // We can't encode A8 bitmaps as PNGs.  Convert them to 8888 first.
            SkBitmap converted;
            if (bitmap->info().colorType() == kAlpha_8_SkColorType) {
                if (!bitmap->copyTo(&converted, kN32_SkColorType)) {
                    fail("Can't convert A8 to 8888.\n");
                    return;
                }
                bitmap = &converted;
            }
            if (!SkImageEncoder::EncodeStream(&file, *bitmap, SkImageEncoder::kPNG_Type, 100)) {
                fail(SkStringPrintf("Can't encode PNG to %s.\n", path.c_str()));
                return;
            }
        } else {
            if (!file.writeStream(data, len)) {
                fail(SkStringPrintf("Can't write to %s.\n", path.c_str()));
                return;
            }
        }
    }
};

// Run all tasks in the same enclave serially on the same thread.
// They can't possibly run concurrently with each other.
static void run_enclave(SkTArray<Task>* tasks) {
    for (int i = 0; i < tasks->count(); i++) {
        Task::Run(tasks->begin() + i);
    }
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

// Unit tests don't fit so well into the Src/Sink model, so we give them special treatment.

static SkTDArray<skiatest::Test> gThreadedTests, gGPUTests;

static void gather_tests() {
    if (!FLAGS_src.contains("tests")) {
        return;
    }
    for (const skiatest::TestRegistry* r = skiatest::TestRegistry::Head(); r;
         r = r->next()) {
        // Despite its name, factory() is returning a reference to
        // link-time static const POD data.
        const skiatest::Test& test = r->factory();
        if (SkCommandLineFlags::ShouldSkip(FLAGS_match, test.name)) {
            continue;
        }
        if (test.needsGpu && gpu_supported()) {
            (FLAGS_gpu_threading ? gThreadedTests : gGPUTests).push(test);
        } else if (!test.needsGpu && FLAGS_cpu) {
            gThreadedTests.push(test);
        }
    }
}

static void run_test(skiatest::Test* test) {
    struct : public skiatest::Reporter {
        void reportFailed(const skiatest::Failure& failure) SK_OVERRIDE {
            fail(failure.toString());
            JsonWriter::AddTestFailure(failure);
        }
        bool allowExtendedTest() const SK_OVERRIDE {
            return FLAGS_pathOpsExtended;
        }
        bool verbose() const SK_OVERRIDE { return FLAGS_veryVerbose; }
    } reporter;
    WallTimer timer;
    timer.start();
    if (!FLAGS_dryRun) {
        GrContextFactory factory;
        test->proc(&reporter, &factory);
    }
    timer.end();
    done(timer.fWall, "unit", "test", test->name, "");
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

// If we're isolating all GPU-bound work to one thread (the default), this function runs all that.
static void run_enclave_and_gpu_tests(SkTArray<Task>* tasks) {
    run_enclave(tasks);
    for (int i = 0; i < gGPUTests.count(); i++) {
        run_test(&gGPUTests[i]);
    }
}

int dm_main();
int dm_main() {
    SetupCrashHandler();
    SkAutoGraphics ag;
    SkTaskGroup::Enabler enabled(FLAGS_threads);
    if (FLAGS_leaks) {
        SkInstCountPrintLeaksOnExit();
    }

    gather_gold();

    gather_srcs();
    gather_sinks();
    gather_tests();

    gPending = gSrcs.count() * gSinks.count() + gThreadedTests.count() + gGPUTests.count();
    SkDebugf("%d srcs * %d sinks + %d tests == %d tasks\n",
             gSrcs.count(), gSinks.count(), gThreadedTests.count() + gGPUTests.count(), gPending);

    // We try to exploit as much parallelism as is safe.  Most Src/Sink pairs run on any thread,
    // but Sinks that identify as part of a particular enclave run serially on a single thread.
    // CPU tests run on any thread.  GPU tests depend on --gpu_threading.
    SkTArray<Task> enclaves[kNumEnclaves];
    for (int j = 0; j < gSinks.count(); j++) {
        SkTArray<Task>& tasks = enclaves[gSinks[j]->enclave()];
        for (int i = 0; i < gSrcs.count(); i++) {
            tasks.push_back(Task(gSrcs[i], gSinks[j]));
        }
    }

    SkTaskGroup tg;
    tg.batch(run_test, gThreadedTests.begin(), gThreadedTests.count());
    for (int i = 0; i < kNumEnclaves; i++) {
        switch(i) {
            case kAnyThread_Enclave:
                tg.batch(Task::Run, enclaves[i].begin(), enclaves[i].count());
                break;
            case kGPU_Enclave:
                tg.add(run_enclave_and_gpu_tests, &enclaves[i]);
                break;
            default:
                tg.add(run_enclave, &enclaves[i]);
                break;
        }
    }
    tg.wait();
    // At this point we're back in single-threaded land.

    SkDebugf("\n");
    if (gFailures.count() > 0) {
        SkDebugf("Failures:\n");
        for (int i = 0; i < gFailures.count(); i++) {
            SkDebugf("\t%s\n", gFailures[i].c_str());
        }
        SkDebugf("%d failures\n", gFailures.count());
        return 1;
    }
    if (gPending > 0) {
        SkDebugf("Hrm, we didn't seem to run everything we intended to!  Please file a bug.\n");
        return 1;
    }
    return 0;
}

#if !defined(SK_BUILD_FOR_IOS) && !defined(SK_BUILD_FOR_NACL)
int main(int argc, char** argv) {
    SkCommandLineFlags::Parse(argc, argv);
    return dm_main();
}
#endif
