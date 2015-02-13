import urllib2, time, threading

URL = "http://localhost:15213"

global systime
def worker(i):
    t = time.time()
    try:
        print "%d starting" % i
        f = urllib2.urlopen(URL + "/cgi-bin/adder%d?%d&%d" % ((i%20)+1,i,40))
    except:
        return
    else:
        print f.read()
        print "Request %d took %0.4f extra s" % (i, time.time() - t)
    f.close()

systime = time.time()
for i in xrange(1,100):
    t = threading.Thread(target=worker, args=(i,))
    t.start()
