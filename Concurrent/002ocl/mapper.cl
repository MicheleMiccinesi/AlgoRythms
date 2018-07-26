__kernel void map(__global TTTT *reduction, __global TTTT* value){
	int id = get_global_id(0);
	reduction[id] = FFFF( reduction[id], *value );
}