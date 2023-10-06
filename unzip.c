#define MINIZ_IMPLEMENTATION
#define MINIZ_NO_STDIO
/* #define MINIZ_NO_MALLOC */
/* #define MINIZ_NO_ARCHIVE_APIS */
#define MINIZ_NO_TIME
#define MINIZ_NO_ZLIB_APIS
#include "miniz.h"

typedef char s8;
typedef unsigned char u8;
typedef unsigned long long int u64;
typedef unsigned int u32;

#define IO_IMPLEMENTATION
#include "io.h"

size_t write_callback(void *opaque, mz_uint64 file_ofs, const void *buf, size_t n) {
  (void) file_ofs;

  size_t read = 0;
  while(read < n) {
    read += io_file_write((Io_File *) opaque, buf, 1, n - read);
  }
  
  return read;
}

size_t read_callback(void *opaque, mz_uint64 file_ofs, void *buf, size_t n) {

  Io_File *f = (Io_File *) opaque;

  u64 pos = (u64) io_file_tell(f);
  if(pos != file_ofs) {
    assert(io_file_seek(f, (long int) file_ofs, SEEK_SET) == 0);
  }
  
  size_t read = 0;
  while(read < n) {
    read += io_file_read(f, buf, 1, n - read);
  }
  return read;
}

int main(int argc, char **argv) {

  if(argc < 3) {
    fprintf(stderr, "ERORR: Please provide enough arguments\n");
    fprintf(stderr, "USAGE: %s <input-file> <output-dir>\n", argv[0]);
    return 1;
  }
  const char *filepath = argv[1];
  const char *output_dir = argv[2];

  bool is_file;
  if(!io_exists(output_dir, &is_file)) {
    fprintf(stderr, "ERORR: '%s' does not exist. Can not unzip in a non existent directory\n",
	    output_dir);
    return 1;
  }
  if(is_file) {
    fprintf(stderr, "ERORR: '%s' is a file. Can not unzip into a file\n",
	    output_dir);
    return 1;
  }

  Io_File f;
  if(!io_file_open(&f, filepath, IO_MODE_READ)) {
    fprintf(stderr, "ERORR: Can not open file '%s': (%d) %s\n",
	    filepath, io_last_error(), io_last_error_cstr());
    return 1;
  }
  u64 f_len;
  if(!io_file_size(&f, &f_len)) {
    fprintf(stderr, "ERORR: Can not get size of file '%s': (%d) %s\n",
	    filepath, io_last_error(), io_last_error_cstr());
    return 1;
  }

  mz_zip_archive archive;
  memset(&archive, 0, sizeof(archive));
  archive.m_pRead = read_callback;
  archive.m_pIO_opaque = &f;
  archive.m_archive_size = f_len;
  
  if(!mz_zip_reader_init(&archive, f_len, 0)) {
    fprintf(stderr, "ERROR: Failed to init archive\n");
    return 1;
  }

  u32 n = mz_zip_reader_get_num_files(&archive);
  for(u32 i=0;i<n;i++) {

    s8 buf[MAX_PATH];
    assert(mz_zip_reader_get_filename(&archive, i, buf, sizeof(buf)) < sizeof(buf));
    s8 path_buf[MAX_PATH];
    assert(snprintf(path_buf, MAX_PATH, "%s\\%s", output_dir, buf) < MAX_PATH);
    printf("%s\n", buf);
    
    if(mz_zip_reader_is_file_a_directory(&archive, i)) {
      if(!io_create_dir(path_buf, NULL)) {
	return 1;
      }
      
    } else {
      Io_File file;
      if(!io_file_open(&file, path_buf, IO_MODE_WRITE)) {
	fprintf(stderr, "ERROR: Failed to open file '%s': (%d) %s",
		buf, io_last_error(), io_last_error_cstr());
	return 1;
      }

      if(!mz_zip_reader_extract_to_callback(&archive, i, write_callback, &file, 0)) {
	fprintf(stderr, "ERROR: Failed to write to file '%s': (%d) %s",
		buf, io_last_error(), io_last_error_cstr());
	return 1;
      }

      io_file_close(&file);

    }
  }

  mz_zip_reader_end(&archive);
  
  io_file_close(&f);
  
  return 0;
}
