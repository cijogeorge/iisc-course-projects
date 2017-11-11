#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  struct dir *dir = extract_directory ((char **) &name);

  if (!dir) 
    return false;
 
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector));
  printf ("\nsuccess: %d", success);
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  if (!strcmp (".", name) || !strcmp ("..", name))
    return NULL;

  struct dir *dir = extract_directory ((char **) &name);
  struct inode *inode = NULL;
  struct dir_entry e;
  
  if (!strcmp (".", name))
  { 
    inode = inode_open (ROOT_DIR_SECTOR);
  } 
   
  else if (dir != NULL)
  {
     dir_lookup (dir, name, &inode, &e);
     if (inode == NULL)
     {
        char *str = malloc (strlen (name) + 6);
        strlcpy (str, "/bin/", 6);
        strlcat (str, name, strlen (name) + 6);
        name = str;
        dir = extract_directory (&str);
        if (dir != NULL)
        {
           dir_lookup (dir, str, &inode, &e);
        }
        free (name);
      }
   }
  
  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = extract_directory ((char **) &name);
  bool success = dir != NULL && dir_remove (dir, name);
  if (thread_current ()->current_directory !=dir && success)
  {
     thread_current ()->current_directory = NULL;
  }

  dir_close (dir); 
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

bool
change_dir (const char *name)
{
  struct file *file = filesys_open (name);
  if (file == NULL)
   {
     return false;
   }
  if (!inode_isdir (file->inode))
   {
     return false;
   }
  struct dir *dir = dir_open (file->inode);
  inode_reopen (file->inode);
  file_close (file);
  dir_close (thread_current ()->current_directory);
  thread_current ()->current_directory = dir;
  return true;
}

struct dir *
extract_directory (char **filename)
{
   struct dir *dir = NULL;
   char *str = malloc (strlen (*filename) + 1);
   strlcpy (str, *filename, strlen (*filename) + 1);

   if (*filename[0] == '/')
    {
      /* Absolute path name. */
      dir = dir_open_root();
      if (str[1] == '\0')
         str[0] = '.';
      else
         str += 1;
    }

   else
    {
      /* Relative path name. */
      if (thread_current ()->current_directory == dir)
        return NULL;
      else
        dir = dir_reopen (thread_current ()->current_directory);
    }
         
   char delim[2] = "/";
   char *token1 = NULL, *token2 = NULL;
   char *saveptr;
    
   token1 = strtok_r (str, delim, &saveptr);
   if (token1 == NULL)
    {
      free (str);
      return NULL;
    }

   while (1)
    {
      str = NULL;
      token2 = strtok_r (str, delim, &saveptr);

      if (token2 != NULL)
       {
         struct dir_entry d_e;
         struct inode *inode = NULL;
         dir_lookup (dir, token1, &inode, &d_e);
         if (inode != NULL)
            dir = dir_open (inode_open (d_e.inode_sector));
         else
          {
            free (str);
            return NULL;       /* Failed lookup. */ 
          }
       }
      else
       {
         *filename = token1;
         free (str);
         return dir;
       }

       token1 = token2;
    }
 }

