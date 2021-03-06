{
  'include_dirs': [
    '../src/core',
    '../src/effects',
    '../src/gpu',
    '../src/utils',
    '../tools',
  ],
  'dependencies': [
    'etc1.gyp:libetc1',
    'skia_lib.gyp:skia_lib',
    'tools.gyp:resources',
    'tools.gyp:sk_tool_utils',
  ],
  'conditions': [
    ['skia_gpu == 1', {
      'include_dirs': [ '../src/gpu' ],
      'dependencies': [ 'gputest.gyp:skgputest' ],
    }],
  ],
  'sources': [
    '../bench/Benchmark.cpp',
    '../bench/Benchmark.h',

    '../bench/AAClipBench.cpp',
    '../bench/AlternatingColorPatternBench.cpp',
    '../bench/BezierBench.cpp',
    '../bench/BigPathBench.cpp',
    '../bench/BitmapBench.cpp',
    '../bench/BitmapRectBench.cpp',
    '../bench/BitmapScaleBench.cpp',
    '../bench/BlurBench.cpp',
    '../bench/BlurImageFilterBench.cpp',
    '../bench/BlurRectBench.cpp',
    '../bench/BlurRectsBench.cpp',
    '../bench/BlurRoundRectBench.cpp',
    '../bench/ChartBench.cpp',
    '../bench/ChecksumBench.cpp',
    '../bench/ChromeBench.cpp',
    '../bench/CmapBench.cpp',
    '../bench/ColorCubeBench.cpp',
    '../bench/ColorFilterBench.cpp',
    '../bench/ColorPrivBench.cpp',
    '../bench/CoverageBench.cpp',
    '../bench/DashBench.cpp',
    '../bench/DecodeBench.cpp',
    '../bench/DeferredSurfaceCopyBench.cpp',
    '../bench/DisplacementBench.cpp',
    '../bench/ETCBitmapBench.cpp',
    '../bench/FSRectBench.cpp',
    '../bench/FontCacheBench.cpp',
    '../bench/FontScalerBench.cpp',
    '../bench/GameBench.cpp',
    '../bench/GeometryBench.cpp',
    '../bench/GrMemoryPoolBench.cpp',
    '../bench/GrResourceCacheBench.cpp',
    '../bench/GrOrderedSetBench.cpp',
    '../bench/GradientBench.cpp',
    '../bench/HairlinePathBench.cpp',
    '../bench/ImageCacheBench.cpp',
    '../bench/ImageDecodeBench.cpp',
    '../bench/ImageFilterDAGBench.cpp',
    '../bench/ImageFilterCollapse.cpp',
    '../bench/InterpBench.cpp',
    '../bench/LightingBench.cpp',
    '../bench/LineBench.cpp',
    '../bench/MagnifierBench.cpp',
    '../bench/MathBench.cpp',
    '../bench/Matrix44Bench.cpp',
    '../bench/MatrixBench.cpp',
    '../bench/MatrixConvolutionBench.cpp',
    '../bench/MemcpyBench.cpp',
    '../bench/MemoryBench.cpp',
    '../bench/MemsetBench.cpp',
    '../bench/MergeBench.cpp',
    '../bench/MipMapBench.cpp',
    '../bench/MorphologyBench.cpp',
    '../bench/MutexBench.cpp',
    '../bench/PatchBench.cpp',
    '../bench/PatchGridBench.cpp',
    '../bench/PathBench.cpp',
    '../bench/PathIterBench.cpp',
    '../bench/PathUtilsBench.cpp',
    '../bench/PerlinNoiseBench.cpp',
    '../bench/PictureNestingBench.cpp',
    '../bench/PicturePlaybackBench.cpp',
    '../bench/PremulAndUnpremulAlphaOpsBench.cpp',
    '../bench/RTreeBench.cpp',
    '../bench/ReadPixBench.cpp',
    '../bench/RectBench.cpp',
    '../bench/RectanizerBench.cpp',
    '../bench/RectoriBench.cpp',
    '../bench/RefCntBench.cpp',
    '../bench/RegionBench.cpp',
    '../bench/RegionContainBench.cpp',
    '../bench/RepeatTileBench.cpp',
    '../bench/RotatedRectBench.cpp',
    '../bench/ScalarBench.cpp',
    '../bench/ShaderMaskBench.cpp',
    '../bench/SkipZeroesBench.cpp',
    '../bench/SortBench.cpp',
    '../bench/StrokeBench.cpp',
    '../bench/TableBench.cpp',
    '../bench/TextBench.cpp',
    '../bench/TileBench.cpp',
    '../bench/VertBench.cpp',
    '../bench/WritePixelsBench.cpp',
    '../bench/WriterBench.cpp',
    '../bench/XfermodeBench.cpp',
  ],
}
