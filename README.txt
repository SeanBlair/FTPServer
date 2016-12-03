
To the person grading my assignment:

I found out too late that the server should be tested against an ftp client,
instead of using telnet. I was very happy that I had achieved all the requirements
of the assignment when using telnet, and feel slightly mislead in that it was not
mentioned that working on telnet is not good enough.

Everything works on telnet as far as i can tell, the only thing I knowingly left out was
implementing a timeout on the data connection. Using an ftp client, PASV seems to work correctly
, but RETR and NLST do not.  Please consider checking their functionality using telnet to give me
some partial credit.

I am very new to C, please forgive the ridiculously long method, since I never finished getting
it all to work, I didn't have time to refactor. 

