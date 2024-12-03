﻿namespace CustomBuildTool
{
    internal sealed class SignCommand
    {
        internal static readonly string CodeSigningOid = "1.3.6.1.5.5.7.3.3";

        private static bool IsAuthenticodeSignrf(string path)
        {
            if (X509Certificate2.GetCertContentType(path) == X509ContentType.Authenticode)
            {
                return true;
            }

            return false;
        }

        private static X509Certificate2Collection GetAdditionalCertificates(IEnumerable<string> paths)
        {
            var collection = new X509Certificate2Collection();
            try
            {
                foreach (var path in paths)
                {
                    var type = X509Certificate2.GetCertContentType(path);
                    switch (type)
                    {
                        case X509ContentType.Cert:
                        case X509ContentType.Authenticode:
                        case X509ContentType.SerializedCert:
                            {
                                var certificate = X509CertificateLoader.LoadCertificateFromFile(path);

                                Program.PrintColorMessage($"Including additional certificate {certificate.Thumbprint}.", ConsoleColor.White);

                                collection.Add(certificate);
                            }
                            break;
                        default:
                            throw new Exception($"Specified file {path} is not a public valid certificate.");
                    }
                }
            }
            catch (Exception e)
            {
                Program.PrintColorMessage($"[ERROR] GetAdditionalCertificates {e}", ConsoleColor.Red);
            }

            return collection;
        }
    }
}
