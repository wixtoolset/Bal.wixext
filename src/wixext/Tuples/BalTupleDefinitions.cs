// Copyright (c) .NET Foundation and contributors. All rights reserved. Licensed under the Microsoft Reciprocal License. See LICENSE.TXT file in the project root for full license information.

namespace WixToolset.Bal
{
    using System;
    using WixToolset.Data;

    public enum BalTupleDefinitionType
    {
        WixBalBAFunctions,
        WixBalCondition,
        WixMbaPrereqInformation,
        WixStdbaOptions,
        WixStdbaOverridableVariable,
    }

    public static partial class BalTupleDefinitions
    {
        public static readonly Version Version = new Version("4.0.0");

        public static IntermediateTupleDefinition ByName(string name)
        {
            if (!Enum.TryParse(name, out BalTupleDefinitionType type))
            {
                return null;
            }

            return ByType(type);
        }

        public static IntermediateTupleDefinition ByType(BalTupleDefinitionType type)
        {
            switch (type)
            {
                case BalTupleDefinitionType.WixBalBAFunctions:
                    return BalTupleDefinitions.WixBalBAFunctions;

                case BalTupleDefinitionType.WixBalCondition:
                    return BalTupleDefinitions.WixBalCondition;

                case BalTupleDefinitionType.WixMbaPrereqInformation:
                    return BalTupleDefinitions.WixMbaPrereqInformation;

                case BalTupleDefinitionType.WixStdbaOptions:
                    return BalTupleDefinitions.WixStdbaOptions;

                case BalTupleDefinitionType.WixStdbaOverridableVariable:
                    return BalTupleDefinitions.WixStdbaOverridableVariable;

                default:
                    throw new ArgumentOutOfRangeException(nameof(type));
            }
        }
    }
}