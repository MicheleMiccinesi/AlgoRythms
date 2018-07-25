#define coorType float

typedef struct{
	unsigned int id;
	coorType dist;
} nearestCentroid;

// to be written (obvious): for N not so small, centroids must be copied to private memory, so... &tc...
__kernel void assignPoint(__global coorType* pts, __global coorType* centroids, __global nearestCentroid* cls, const int K, const int D){
	int id = get_global_id(0);

	coorType dist, temp;
	coorType minDist;
	int minId;
	int bPts = id*(D+1), iCen;
	int ePts = bPts+D;
	minDist = 0;
	minId = 0;
	iCen = 0;
	for( int iPts=bPts; iPts<ePts; ++iPts, ++iCen ){
		temp = pts[iPts]-centroids[iCen];
		minDist += temp*temp;
	}
	++iCen;
	for( int k=1; k<K; ++k, ++iCen ){
		dist = 0;			
		for( int iPts=bPts; iPts<ePts; ++iPts, ++iCen ){
			temp = pts[iPts]-centroids[iCen];
			dist += temp*temp;
		}
		if( dist<minDist ){
			minDist = dist;
			minId = k;
		}
	}
	cls[id].id = minId;
	cls[id].dist = minDist;
}

__kernel void assignPointSliced(__global coorType* pts, __global coorType* centroids, __global nearestCentroid* cls, const int K, const int D, const int N, const int nPts){
	int id = get_global_id(0);
	int b = N*id;
	int e = b+N;
	if( nPts<e )
		e = nPts;

	coorType dist, temp;
	coorType minDist;
	int minId;
	int bPts = b*(D+1), iCen;
	int ePts = bPts+D;
	for( int i=b; i<e; ++i, bPts+=D+1, ePts+=D+1){
		minDist = 0;
		minId = 0;
		iCen = 0;
		for( int iPts=bPts; iPts<ePts; ++iPts, ++iCen ){
			temp = pts[iPts]-centroids[iCen];
			minDist += temp*temp;
		}
		++iCen;
		for( int k=1; k<K; ++k, ++iCen ){
			dist = 0;			
			for( int iPts=bPts; iPts<ePts; ++iPts, ++iCen ){
				temp = pts[iPts]-centroids[iCen];
				dist += temp*temp;
			}
			if( dist<minDist ){
				minDist = dist;
				minId = k;
			}
		}
		cls[i].id = minId;
		cls[i].dist = minDist;
	}
}