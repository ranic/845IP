import urllib2

f = urllib2.urlopen("http://localhost:15210/cgi-bin/adder?5&10")
print f.read()
print "Hello world!"
f.close()
