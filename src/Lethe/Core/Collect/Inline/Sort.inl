template< typename T, typename S >
template< typename C >
void QuickSort<T,S>::Sort(T *ptr, S size, const C &cmp)
{
	LETHE_ASSERT(!size || ptr);
	Sort(ptr, 0, size-1, cmp);
}

template< typename T, typename S >
template< typename C >
void QuickSort<T,S>::Sort(T *ptr, const T *end, const C &cmp)
{
	LETHE_ASSERT(ptr && end >= ptr);
	Sort(ptr, 0, (S)(end - ptr)-1, cmp);
}

template< typename T, typename S >
inline S QuickSort<T,S>::PickPivot(S start, S end)
{
	// simply pick the middle index, should favor already sorted lists
	S mid = (start+end) >> 1;
	return mid;
}

template< typename T, typename S >
template< typename C >
S QuickSort<T,S>::Partition(T *ptr, S start, S end, const C &cmp)
{
	if (start >= end)
		return end;

	// we do this: pick start, end and mid and choose best
	S pivot = PickPivot(start, end);
	// temporarily move pivot to the end of the list (=> unstable)
	Swap(ptr[pivot], ptr[end]);

	S partIndex = start;

	for (S i=start; i < end; i++)
	{
		if (!cmp(ptr[end], ptr[i]))
			Swap(ptr[i], ptr[partIndex++]);
	}

	// move pivot back in place
	Swap(ptr[end], ptr[partIndex]);
	return partIndex;
}

// space-friendly version
template< typename T, typename S >
template< typename C >
void QuickSort<T,S>::Sort(T *ptr, S start, S end, const C &cmp)
{
	while (start < end)
	{
		if (end - start < INSERTION_THRESHOLD)
		{
			InsertionSort<T, S>::Sort(ptr + start, end - start + 1, cmp);
			return;
		}

		S p = Partition(ptr, start, end, cmp);
		S l = p-1;
		S h = p+1;

		// note: due to the way partitioning works, all element equal to pivot are partitioned before it,
		// so one loop will do here
		while (l > start && !cmp(ptr[l], ptr[p]))
			l--;

		// doing start, l and h, end
		if (l-start < end-h)
		{
			Sort(ptr, start, l, cmp);
			start = h;
		}
		else
		{
			Sort(ptr, h, end, cmp);
			end = l;
		}
	}
}

template< typename T, typename S >
template< typename C >
void InsertionSort<T,S>::Sort(T *ptr, S size, const C &cmp, S start)
{
	LETHE_ASSERT(!size || ptr);

	for (S i=start; i<size; i++)
	{
		S j = i;

		while (j > 0 && cmp(ptr[j], ptr[j-1]))
		{
			Swap(ptr[j], ptr[j-1]);
			j--;
		}
	}
}

template< typename T, typename S >
template< typename C >
void InsertionSort<T,S>::Sort(T *ptr, const T *end, const C &cmp)
{
	LETHE_ASSERT(end >= ptr);
	Sort(ptr, (S)(end - ptr), cmp);
}

// MergeSort

template< typename T, typename S >
template< typename C >
void MergeSort<T,S>::Merge(T *ptr, S mid, S size, T *scratch, const C &cmp)
{
	S sptr = 0;
	S x0 = 0;
	S x1 = mid;

	while (x0 < mid && x1 < size)
		scratch[sptr++] = !cmp(ptr[x1], ptr[x0]) ? ptr[x0++] : ptr[x1++];

	while (x0 < mid)
		scratch[sptr++] = ptr[x0++];

	while (x1 < size)
		scratch[sptr++] = ptr[x1++];

	// finally copy back from scratch pad
	for (S i=0; i<sptr; i++)
		ptr[i] = scratch[i];
}

template< typename T, typename S >
template< typename C >
void MergeSort<T,S>::SortRecursive(T *ptr, S size, T *scratch, const C &cmp)
{
	if (size <= INSERTION_THRESHOLD)
	{
		InsertionSort<T,S>::Sort(ptr, size, cmp);
		return;
	}

	LETHE_ASSERT(ptr);
	S mid = size/2;
	SortRecursive(ptr, mid, scratch, cmp);
	SortRecursive(ptr + mid, size - mid, scratch, cmp);
	Merge(ptr, mid, size, scratch, cmp);
}

template< typename T, typename S >
template< typename C >
void MergeSort<T,S>::Sort(T *ptr, S size, const C &cmp)
{
	if (size <= 0)
		return;

	LETHE_ASSERT(ptr);

	const size_t scratchBufSize = 8000/sizeof(T);
	T scratchBuf[scratchBufSize];
	T *scratch = scratchBufSize >= size ? scratchBuf : new T[size];
	LETHE_ASSERT(scratch);
	SortRecursive(ptr, size, scratch, cmp);

	if (scratch != scratchBuf)
		delete[] scratch;
}

template< typename T, typename S >
template< typename C >
void MergeSort<T,S>::Sort(T *ptr, const T *end, const C &cmp)
{
	LETHE_ASSERT(end >= ptr);
	Sort(ptr, (S)(end - ptr), cmp);
}

// InplaceMergeSort

// left rotation by mid
template< typename T, typename S >
inline void InplaceMergeSort<T,S>::Rotate(T *buf, S mid, S len)
{
	RotateArray(buf, buf + mid, buf + len);
}

template< typename T, typename S >
template< typename C >
void InplaceMergeSort<T,S>::Merge(T *buf, S mid, S size, T *sbuf, const C &cmp)
{
	if (mid <= 0 || mid >= size || size <= 1)
		return;

	// handle one very lucky case
	if (!cmp(buf[mid], buf[mid-1]))
		return;

	if (mid <= SCRATCH_SIZE && mid <= size - mid)
	{
		// we can do better here:
		// copy out small part to scratch buffer, then merge into buf
		// first saw this idea in TimSort description
		// unfortunately this only gives negligible speedup

		for (S i=0; i<mid; i++)
			sbuf[i] = buf[i];

		S x0 = 0;
		S x1 = mid;
		S dst = 0;

		while (x0 < mid && x1 < size)
			buf[dst++] = !cmp(buf[x1], sbuf[x0]) ? sbuf[x0++] : buf[x1++];

		// only either of conditions satisfied
		while (x0 < mid)
			buf[dst++] = sbuf[x0++];

		while (x1 < size)
			buf[dst++] = buf[x1++];

		LETHE_ASSERT(dst == size);
		return;
	}

	S right = size - mid;

	if (right <= SCRATCH_SIZE && right <= mid)
	{
		// copy right part to scratch buffer, then merge into buf
		// however, we have to use inverse=> merge

		for (S i=0; i<right; i++)
			sbuf[i] = buf[mid + i];

		S x0 = mid-1;
		S x1 = right-1;
		S dst = size-1;

		while (x0 >= 0 && x1 >= 0)
		{
			// inverse condition
			buf[dst--] = cmp(sbuf[x1], buf[x0]) ? buf[x0--] : sbuf[x1--];
		}

		// only either of conditions satisfied
		while (x0 >= 0)
			buf[dst--] = buf[x0--];

		while (x1 >= 0)
			buf[dst--] = sbuf[x1--];

		LETHE_ASSERT(dst == -1);
		return;
	}

	if (size <= SCRATCH_SIZE)
	{
		// use fast copy
		S x0 = 0;
		S x1 = mid;
		S dst = 0;

		while (x0 < mid && x1 < size)
			sbuf[dst++] = buf[!cmp(buf[x1], buf[x0]) ? x0++ : x1++];

		// only either of conditions satisfied
		while (x0 < mid)
			sbuf[dst++] = buf[x0++];

		while (x1 < size)
			sbuf[dst++] = buf[x1++];

		LETHE_ASSERT(dst == size);

		for (S i=0; i<dst; i++)
			buf[i] = sbuf[i];

		return;
	}

	S mid1, mid2;

	if (mid >= right)
	{
		// lookup midval in right part
		mid1 = mid >> 1;
		const T *ptr = LowerBound(buf + mid, buf + size, buf[mid1], cmp);
		mid2 = (S)(ptr - buf);
	}
	else
	{
		mid2 = (mid + size) >> 1;
		const T *ptr = UpperBound(buf, buf + mid, buf[mid2], cmp);
		mid1 = (S)(ptr - buf);
	}

	LETHE_ASSERT(mid1 >= 0 && mid1 <= mid && mid <= mid2 && mid2 <= size);

	Rotate(buf + mid1, mid - mid1, mid2 - mid1);
	S tmp = mid1 + (mid2 - mid);
	Merge(buf, mid1, tmp, sbuf, cmp);
	Merge(buf + tmp, mid2 - tmp, size - tmp, sbuf, cmp);
}

template< typename T, typename S >
template< typename C >
void InplaceMergeSort<T,S>::Sort(T *buf, S size, const C &cmp)
{
	T scratch[SCRATCH_SIZE];
	SortInternal(buf, size, scratch, cmp);
}

template< typename T, typename S >
template< typename C >
void InplaceMergeSort<T,S>::Sort(T *buf, const T *end, const C &cmp)
{
	T scratch[SCRATCH_SIZE];
	SortInternal(buf, (S)(end - buf), scratch, cmp);
}

template< typename T, typename S >
template< typename C >
void InplaceMergeSort<T,S>::SortInternal(T *buf, S size, T *sbuf, const C &cmp)
{
	if (size <= INSERTION_THRESHOLD)
	{
		InsertionSort<T,S>::Sort(buf, size, cmp);
		return;
	}

	S mid = size/2;
	SortInternal(buf, mid, sbuf, cmp);
	SortInternal(buf + mid, size - mid, sbuf, cmp);
	Merge(buf, mid, size, sbuf, cmp);
}
