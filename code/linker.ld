SECTIONS
{
  .gnu_build_id : 
  {
    PROVIDE(g_note_build_id = .);
    *(.note.gnu.build-id)
  }
}
INSERT AFTER .rodata;
