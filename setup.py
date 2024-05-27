from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup
import sys
import os

__version__ = "0.0.1"

def determine_inc_lib():
    # Determine the include and lib directories
    if sys.platform == 'win32':
        include_dirs = [r'c:\bin\boost_1_85_0', r'C:\bin\QuantLib-1.34']
        library_dirs = [r'C:\bin\QuantLib-1.34\lib']
    else:
        include_dirs = [r'/usr/local/boost182/include', r'/usr/include/boost', r'/usr/include/QuantLib]
        library_dirs = [r'']
    return include_dirs, library_dirs

inc_dirs, lib_dirs = determine_inc_lib()

ext_modules = [
    Pybind11Extension(
        'TaxCalculator',
        sources=['TaxCalculator.cpp'],
        include_dirs=inc_dirs,
        library_dirs=lib_dirs,
        libraries=['QuantLib'],
        language='c++',
        cxx_std=11
    ),
]

setup(
    name='TaxCalculator',
    version=__version__,
    author='A B',
    install_requires=['pandas', 'numpy'],
    ext_modules=ext_modules,
    # cmdclass={'build_ext': build_ext},
    include_package_data=True,
    url='',
    license='None',
    description='Tax Calculator',
)