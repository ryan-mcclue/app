// SPDX-License-Identifier: zlib-acknowledgement

introspect("hi") struct Person
{
  u32 age;
  f32 weight;
  String8 name;
  u32 count;
  counted_pointer(count) u32 *array;
};
