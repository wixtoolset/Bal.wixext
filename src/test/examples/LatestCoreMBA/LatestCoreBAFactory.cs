// Copyright (c) .NET Foundation and contributors. All rights reserved. Licensed under the Microsoft Reciprocal License. See LICENSE.TXT file in the project root for full license information.

[assembly: WixToolset.Mba.Core.BootstrapperApplicationFactory(typeof(Example.LatestCoreMBA.LatestCoreBAFactory))]
namespace Example.LatestCoreMBA
{
    using WixToolset.Mba.Core;

    public class LatestCoreBAFactory : BaseBootstrapperApplicationFactory
    {
        protected override IBootstrapperApplication Create(IEngine engine, IBootstrapperCommand bootstrapperCommand)
        {
            return new LatestCoreBA(engine);
        }
    }
}
