// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

// Convenience header file for including commonly needed engine classes. Note: intentionally does not include Debug/DebugNew.h
// so that placement new works as expected.

#include "Base/AutoPtr.h"
#include "Base/HashMap.h"
#include "Base/List.h"
#include "Base/SharedPtr.h"
#include "Base/WeakPtr.h"
#include "Math/Color.h"
#include "Math/Frustum.h"
#include "Math/Matrix3.h"
#include "Math/Polyhedron.h"
#include "Math/Ray.h"
#include "Math/StringHash.h"
#include "Thread/Condition.h"
#include "Thread/Mutex.h"
#include "Thread/Thread.h"
#include "Thread/ThreadLocalValue.h"