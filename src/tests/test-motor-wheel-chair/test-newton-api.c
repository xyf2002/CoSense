/*
	Authored 2017. Jonathan Lim.

	All rights reserved.


	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:

	*	Redistributions of source code must retain the above
		copyright notice, this list of conditions and the following
		disclaimer.

	*	Redistributions in binary form must reproduce the above
		copyright notice, this list of conditions and the following
		disclaimer in the documentation and/or other materials
		provided with the distribution.

	*	Neither the name of the author nor the names of its
		contributors may be used to endorse or promote products
		derived from this software without specific prior written
		permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "flextypes.h"
#include "flexerror.h"
#include "flex.h"

#include "newton-timeStamps.h"
#include "common-timeStamps.h"
#include "common-data-structures.h"
#include "newton-data-structures.h"
#include "common-irHelpers.h"
#include "common-lexers-helpers.h"
#include "newton-parser-expression.h"
#include "newton-lexer.h"
#include "newton-symbolTable.h"
#include "newton-api.h"

#include "minunit.h"
#include "test-utils.h"
#include "test-newton-api.h"
#include "probes.h"
#include "invariant-probes.h"

extern int tests_run;



char * test_newtonApiInit_notNull()
{
    NEWTON_NEWTON_START();
	mu_assert(
        "test_newtonApiInit_notNull: newtonApiInit returns NULL!",
        newtonApiInit("../../Examples/motor_wheel_chair.nt") != NULL
    );
    NEWTON_NEWTON_DONE();
    return 0;
}

char * test_newtonApiInit_notNullInvariant()
{
    State* newton = newtonApiInit("../../Examples/motor_wheel_chair.nt");
	mu_assert(
        "test_newtonApiInit_notNullInvariant: invariantList for motor_wheel_chair.nt is NULL!",
         newton->invariantList != NULL
    );

    return 0;
}

char * test_newtonApiGetPhysicsTypeByNameAndSubindex_Valid()
{
    State* newton = newtonApiInit("../../Examples/motor_wheel_chair.nt");
    mu_assert(
        "newtonApiGetPhysicsTypeByNameAndSubindex: acceleration on Z axis not found",
        newtonApiGetPhysicsTypeByNameAndSubindex(newton, "acceleration", 2) != NULL
		);

    return 0;
}

char * test_newtonApiGetPhysicsTypeByName_Valid()
{
    State* newton = newtonApiInit("../../Examples/motor_wheel_chair.nt");

    mu_assert(
        "newtonApiGetPhysicsTypeByName: mass not found",
        !strcmp(
            newtonApiGetPhysicsTypeByName(newton, "mass")->identifier,
            "mass"
        )
    );

    mu_assert(
        "newtonApiGetPhysicsTypeByName: pressure not found",
        !strcmp(
            newtonApiGetPhysicsTypeByName(newton, "pressure")->identifier,
            "pressure"
        )
    );

    return 0;
}

char * test_newtonApiGetInvariantByParameters_Valid()
{
    State* newton = newtonApiInit("../../Examples/motor_wheel_chair.nt");

    mu_assert(
        "test_newtonApiGetInvariantByParameters: the invariant is named SafetyCheck",
        !strcmp(
            newtonApiGetInvariantByParameters(
                newton,
                newton->invariantList->parameterList
            )->identifier,
            "SafetyCheck"
        )
    );

    return 0;
}

char * test_newtonCheckSingleInvariant()
{
	State * newton = newtonApiInit("../../Examples/motor_wheel_chair.nt");
	IrNode* parameter = makeTestParameterTuple(newton);
    INVARIANT_INVARIANT_START();
    NewtonAPIReport* report = newtonApiSatisfiesConstraints(
				newton,
                parameter
				);
    INVARIANT_INVARIANT_DONE();
	mu_assert(
		"test_newtonCheckSingleInvariant motor_wheel_chair.nt: number passed should be 3",
	    numberOfConstraintsPassed(
            report
			) == 3
		);

	return 0;
}

