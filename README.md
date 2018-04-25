# FD_Plate
C++ Finite difference plate

This code illustrates a very basic finidt difference plate scheme.  
Code has been left explicit to make it easier to follow while audio read-in and playback  
functions have been included in the AudioOut.h file.
  
All code tested with Xcode Version 8.2.1 (8C1002)  
4.2.1 Compatible Apple LLVM 8.0.0 (clang-800.0.42.1)

Values inbetween the 'Edit Here' banners can be changed  
  
If running executable from command line, the first arguement is the output file name.
  
alut.h library deprecated in macOS: but you can use freealut.  

In Terminal  
`\>\> brew install freealut`
  
Don't forget to add the AL folder in the header search path  
  
`/usr/local/Cellar/freealut/1.1.0/include`  
  
The `#ifdef` guard should flag up if ALUT has not been included
