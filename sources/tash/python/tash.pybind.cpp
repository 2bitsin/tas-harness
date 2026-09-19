#include "tash/python/bindings.hpp"

#include <pybind11/pybind11.h>

PYBIND11_MODULE(tash, module)
{
  tash::python::BindTash(module);
}
