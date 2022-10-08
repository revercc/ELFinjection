# ELFinjection
ELFinjection is a simple utility for inject so, it can do the following:

* Using ptrace to inject a so file into the process
  ```
  ./ELFinjection -p  <pid>  <libpath> 
  ```
* Use ptrace to inject a so file into the process and execute a remote function
  ```
  ./ELFinjection -p  <pid>  <libpath>  <funcname>
  ```
* Add a declared dependency on a dynamic library (DT_NEEDED)
  ```
  ./ELFinjection -e  <elf_path>  <so_path>
  ```
  example:
  ![](c:/Users/Administrator.DESKTOP-BA7RGS4/AppData/Roaming/Tencent/Users/2535836763/QQ/WinTemp/RichOle/AG3XPYHJPTE4%5B%5DV7L_U~BT2.png)

  readelf -d 
  ![](c:/Users/Administrator.DESKTOP-BA7RGS4/AppData/Roaming/Tencent/Users/2535836763/QQ/WinTemp/RichOle/8B@@X%5B_O7U5R)P@CYUV93%60U.png)