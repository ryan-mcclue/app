GLOBAL MemberDefinition g_members_of_Person[] = 
{
    {meta_type_u32, "age", (uintptr_t)&(((Person *)0)->age)}, 
    {meta_type_f32, "weight", (uintptr_t)&(((Person *)0)->weight)}, 
    {meta_type_String8, "name", (uintptr_t)&(((Person *)0)->name)}, 
};
#define META_STRUCT_DUMP(member_ptr) \ 
case meta_type_Person: dump_struct(member_ptr, members_of_Person, ARRAY_COUNT(members_of_Person))
