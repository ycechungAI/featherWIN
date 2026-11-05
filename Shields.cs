using Microsoft.Web.WebView2.Core;

namespace FeatherWin;

// Ad and tracker blocking for WebView2. Requests that are not the page itself (images, scripts,
// frames, XHR) are answered with 403 when their host is on the blocklist or under one of its domains.
// The list here is a starting point; Feather's own rules (Shields.swift) are the model for a full list.
public sealed class Shields
{
    private static readonly string[] BlockedDomains =
    {
        "doubleclick.net",
        "googlesyndication.com",
        "googleadservices.com",
        "adservice.google.com",
        "adnxs.com",
        "criteo.com",
        "outbrain.com",
        "taboola.com",
        "scorecardresearch.com",
        "hotjar.com",
    };

    public bool Enabled { get; set; } = true;

    public void Attach(CoreWebView2 core)
    {
        core.AddWebResourceRequestedFilter("*", CoreWebView2WebResourceContext.All);
        core.WebResourceRequested += (_, e) =>
        {
            if (!Enabled || e.ResourceContext == CoreWebView2WebResourceContext.Document)
            {
                return;
            }
            if (Uri.TryCreate(e.Request.Uri, UriKind.Absolute, out var uri) && IsBlocked(uri.Host))
            {
                e.Response = core.Environment.CreateWebResourceResponse(null, 403, "Blocked", "");
            }
        };
    }

    public static bool IsBlocked(string host)
    {
        foreach (var domain in BlockedDomains)
        {
            if (host == domain || host.EndsWith("." + domain, StringComparison.OrdinalIgnoreCase))
            {
                return true;
            }
        }
        return false;
    }
}
