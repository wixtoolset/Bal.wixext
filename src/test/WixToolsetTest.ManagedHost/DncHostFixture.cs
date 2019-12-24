// Copyright (c) .NET Foundation and contributors. All rights reserved. Licensed under the Microsoft Reciprocal License. See LICENSE.TXT file in the project root for full license information.

namespace WixToolsetTest.ManagedHost
{
    using WixBuildTools.TestSupport;
    using Xunit;

    public class DncHostFixture
    {
        [Fact]
        public void CanLoadEarliestCoreMBA()
        {
            var testEngine = new TestEngine();
            var baFile = TestData.Get(@"..\examples\publish\Example.EarliestCoreMBA\dnchost.dll");

            var result = testEngine.RunShutdownEngine(baFile);
            var logMessages = result.Output;
            Assert.Equal("Loading managed bootstrapper application.", logMessages[0]);
            Assert.Equal("Creating BA thread to run asynchronously.", logMessages[1]);
            Assert.Equal("EarliestCoreBA", logMessages[2]);
            Assert.Equal("Shutdown,ReloadBootstrapper,0", logMessages[3]);
        }

        [Fact]
        public void CanLoadLatestCoreMBA()
        {
            var testEngine = new TestEngine();
            var baFile = TestData.Get(@"..\examples\publish\Example.LatestCoreMBA\dnchost.dll");

            var result = testEngine.RunShutdownEngine(baFile);
            var logMessages = result.Output;
            Assert.Equal("Loading managed bootstrapper application.", logMessages[0]);
            Assert.Equal("Creating BA thread to run asynchronously.", logMessages[1]);
            Assert.Equal("LatestCoreBA", logMessages[2]);
            Assert.Equal("Shutdown,ReloadBootstrapper,0", logMessages[3]);
        }
    }
}
