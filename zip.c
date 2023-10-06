#include <stdio.h>

#define IO_IMPLEMENTATION
#include "io.h"

#define MINIZ_IMPLEMENTATION
#include "miniz.h"

size_t write_callback(void *opaque, mz_uint64 file_ofs, const void *buf, size_t n) {
  (void) file_ofs;

  size_t read = 0;
  while(read < n) {
    read += io_file_write((Io_File *) opaque, buf, 1, n - read);
  }
  
  return read;
}

const char *content = "this is content";

size_t read_callback(void *opaque, mz_uint64 file_ofs, void *buf, size_t n) {

  Io_File *f = (Io_File *) opaque;

  size_t pos = (size_t) io_file_tell(f);
  if(pos != file_ofs) {
    assert(io_file_seek(f, (long int) file_ofs, SEEK_SET) == 0);
  }

  return io_file_read(f, buf, 1, n);
}

bool archive_file(mz_zip_archive *archive, const char *zip_name, const char *file_path) {
  Io_File file;
  if(!io_file_open(&file, file_path, IO_MODE_READ)) {
    fprintf(stderr, "ERORR: Can not open '%s': (%d) %s\n",
	    file_path, io_last_error(), io_last_error_cstr());
    return false;
  }
            
  printf("%s\n", zip_name);
    
  size_t size;
  if(!io_file_size(&file, &size)) {
    fprintf(stderr, "ERORR: Can not get size of '%s': (%d) %s\n",
	    file_path, io_last_error(), io_last_error_cstr());
    return false;
  }

  if(!mz_zip_writer_add_read_buf_callback(archive, zip_name, read_callback, &file, size,
					  NULL, NULL, 0,
					  MZ_DEFAULT_COMPRESSION,
					  NULL, 0,
					  NULL, 0)) {
    fprintf(stderr, "ERROR: Failed to add to archive\n");
    return false;
  }

  io_file_close(&file);

  return true;
}

// TODO: maybe close everything nicely on error
bool archive_dir_impl(mz_zip_archive *archive, size_t off, const char *dir_path, const char *exclude) {
  Io_Dir dir;
  if(!io_dir_open(&dir, dir_path)) {
    fprintf(stderr, "ERORR: Can not open dir '%s': (%d) %s\n",
	    dir_path, io_last_error(), io_last_error_cstr());
    return false;
  }

  Io_Dir_Entry entry;
  while(io_dir_next(&dir, &entry)) {

    if(entry.name && *entry.name == '.') continue;
    if(strcmp(entry.name, exclude) == 0) continue;
    
    if(entry.is_dir) {
      archive_dir_impl(archive, off, entry.abs_name, exclude);
    } else {
      assert(strlen(entry.abs_name) > off);

      if(!archive_file(archive, entry.abs_name + off, entry.abs_name)) {
	return false;
      }
    }

  }  

  io_dir_close(&dir);

  return true;
}

#define archive_dir(a, d, o) archive_dir_impl((a), strlen(d), (d), (o))

int main(int argc, char **argv) {

  if(argc < 3) {
    fprintf(stderr, "ERROR: Provide enough arguments\n");
    fprintf(stderr, "USAGE: %s <input-file/folder> <output-zip>\n", argv[0]);
    return 1;
  }
  const char *input = argv[1];
  const char *output = argv[2]; 
  
  bool is_file;
  if(!io_exists(input, &is_file)) {
    fprintf(stderr, "ERORR: Can not find file '%s'\n", input);
    return 1;
  }

  Io_File f;
  if(!io_file_open(&f, output, IO_MODE_WRITE)) {
    return 1;
  }  

  mz_zip_archive archive;
  memset(&archive, 0, sizeof(archive));
  archive.m_pWrite = write_callback;
  archive.m_pIO_opaque = &f;

  if(!mz_zip_writer_init(&archive, 0)) {
    fprintf(stderr, "ERROR: Failed to initialize writer\n");
    return 1;
  }

  if(is_file) {
    if(!archive_file(&archive, input, input)) {
      fprintf(stderr, "ERROR: Failed to archive file '%s'\n", input);
      return 1;
    }
  } else {

    // Maybe append '/' to directory
    char path[IO_MAX_PATH];
    size_t input_len = strlen(input);
    memcpy(path, input, input_len + 1);
    if(output[input_len - 1] != '/') {
      path[input_len ] = '/';
      path[input_len + 1] = 0;
    }
    
    if(!archive_dir(&archive, path, output)) {
      fprintf(stderr, "ERROR: Failed to archive dir '%s'\n", input);
      return 1;
    }
    
  }
  
  if(!mz_zip_writer_finalize_archive(&archive)) {
    fprintf(stderr, "ERROR: Failed to finalize\n");
    return 1;
  }
  
  if(!mz_zip_writer_end(&archive)) {
    fprintf(stderr, "ERROR: Failed to end\n");
    return 1;
  }

  io_file_close(&f);
  
  return 0;
}
