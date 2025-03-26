# zScalerTCPTest
Test application for pushing a simple TCP stream with hash verification.  

## Background
After users started reporting errors in a location utilizing zScaler on Windows 11 we found that simple TCP streams were getting corrupted with additional data in the stream that was a repeat of an earlier data segment.  The repeat can occur multiple times depending on the length of the stream.  Usually the first repeat occurs within the first 100KB of data.
The error doesn't appear to occur in Windows 10 with zScaler, only Windows 11 with zScaler.  All OS versions without zScaler are obviously fine.

## Sample
The sample code demonstrates a simple TCP communication between client and server. After an initial fixed size descriptor is sent a variable length data stream is sent. On completion the hash value and length of transmission is displayed.  In the failure case the length will be different (greater than expected), and obviously the calcuated hash will also be incorrect.
The "Receiver" can be run on any Windows OS version, Windows 10, Server OS etc.  

