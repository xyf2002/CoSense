char * test_newtonApiInit_notNull();
char * test_newtonApiInit_notNullInvariant();
char * test_newtonApiGetInvariantByParameters_Valid();
char * test_newtonApiGetPhysicsTypeByName_Valid();
char * test_newtonApiPhysicsTypeUsageExample();
char * test_newtonCheckSingleInvariant();
char * test_newtonApiNumberParametersZeroToN();

/* just a small helper method */
NoisyIrNode *
makeNoisyIrNodeSetValue(
                        NoisyState * N,
                        NoisyIrNodeType nodeType,
                        char * identifier,
                        double realConst
                        );

NoisyIrNode * makeTestParameterTuple();
