from setuptools import setup
from torch.utils.cpp_extension import CUDAExtension, BuildExtension

setup(
    name='sovereign_cuda_extension',
    ext_modules=[
        CUDAExtension(
            'sovereign_cuda_extension',
            ['sovereign_cuda_extension.cpp'],
            extra_compile_args={
                'cxx': ['-O3'],
                'nvcc': ['-O3', '-arch=sm_61', '-allow-unsupported-compiler'],
            },
            libraries=[':libsovereign_cuda.so', ':librmsnorm_bwd.so',
                       ':libsparse_4k.so', ':libsparse_bwd_4k.so', ':libattn2.so'],
            library_dirs=['/data/模型训练精度验证/phase3/ggml/cuda'],
        ),
    ],
    cmdclass={'build_ext': BuildExtension},
)
