def binsearch(A, x):
    """ Returns the index of x in A, or -1 if not found """
    left, right = 0, len(A)
    while (left < right):
        mid = (left + right)/2
        if (x > A[mid]):
            left = mid+1
        elif (x == A[mid]):
            return mid
        else:
            right = mid
    return -1

print binsearch([1,2,3],2)
print binsearch([1,2,3],1)
print binsearch([1,2,3],3)
print binsearch([1,2,3,4],1)
print binsearch([1,2,3,4],2)
print binsearch([1,2,3,4],3)
print binsearch([1,2,3,4],4)
