/**********************************************************************
 * File callScript.c
 *
 * call the given script with root rights
 *
 * In order to make this secure, the script files must be
 * owned by root and be have the right r-x------
 *
 * this file must be owned by root and the right must be
 * set uid bit must be set.
 *
 **********************************************************************/
 
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
   struct stat status;
   int    error = 0;
   char   *nm;

   setuid(0);
   setgid(0);

   /* check for arguments */
   if ( argc > 1 )
   {
      /* the first one must be the script name and
       * the rights of the script must be OK:
       * owner root read and execute right only for
       * the owner.
       */

      if ( stat(argv[1], &status) == 0 )
      {
         if ( S_ISREG(status.st_mode )
              &&
              (status.st_mode & (S_IRWXO|S_IRWXG|S_IRWXU)) == (S_IRUSR|S_IXUSR)
              &&
              status.st_uid  == 0
            )
         {
            nm = argv[0];
            argv[0] = "/bin/sh";
            /* execute the script */
            error = execv(argv[0], argv);
            if ( error )
               perror(nm);
         }
         else
         {
            printf("%s: No right to do this\n", argv[0]);
            error = 1;
         }
      }
      else
      {
          /* file not found ? */
         perror(argv[0]);
         error = 1;
      }
   }
   else
   {
      printf("Syntax: %s script [args...] \n",argv[0]);
      error = 1;
   }

   return error;
}
