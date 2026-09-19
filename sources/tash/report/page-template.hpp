#pragma once
// Every tag the report writes lives here, as templates with `{{slot}}`
// holes; render.cpp only decides what goes in them.

#include <string_view>

namespace tash::report::detail::page_template
{
  inline constexpr std::string_view PAGE{ R"PAGE(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>tash report {{name}}</title>
<style>
:root { color-scheme: dark; }
body { background: #14161a; color: #d8dee9; margin: 0 auto; max-width: 60rem;
       padding: 1.5rem 1rem 4rem;
       font: 15px/1.5 ui-monospace, "Cascadia Code", Menlo, monospace; }
h1 { font-size: 1.3rem; margin: 0 0 .2rem; }
h2 { font-size: 1rem; margin: 2rem 0 .5rem; color: #88c0d0;
     border-bottom: 1px solid #2b3038; padding-bottom: .2rem; }
a { color: #88c0d0; }
table { border-collapse: collapse; width: 100%; }
td, th { text-align: left; padding: .15rem .6rem .15rem 0;
         vertical-align: top; }
.manifest td:first-child { color: #7b8494; width: 12rem; }
video { width: min(100%, 40rem); background: #000;
        margin-top: .5rem; display: block; }
ol, ul { list-style: none; margin: 0; padding: 0; }
li { padding: .2rem .4rem; border-left: 3px solid #2b3038; }
li[data-at] { cursor: pointer; }
li[data-at]:hover { background: #1d2026; }
.pass { border-left-color: #a3be8c; }
.fail { border-left-color: #bf616a; }
.mark { border-left-color: #5e81ac; }
.group { border-left-color: #b48ead; }
.rewind { border-left-color: #d08770; }
.reset { border-left-color: #ebcb8b; }
.frame { color: #7b8494; display: inline-block; min-width: 5rem; }
.at { color: #7b8494; display: inline-block; min-width: 5rem; }
.outcome { color: #7b8494; }
.name { color: #ebcb8b; }
li.pass .word { color: #a3be8c; }
li.fail .word { color: #bf616a; }
.plot { width: 100%; height: 72px; background: #1a1d22; display: block; }
.plot polyline { fill: none; stroke: #88c0d0; stroke-width: 1; }
.ruler { width: 100%; height: 22px; background: #1a1d22; display: block; }
.ruler line { stroke-width: 1; }
figcaption { color: #7b8494; margin-top: .8rem; }
.tail { font-size: 13px; }
.tail td { white-space: pre; }
.tail .kind { color: #ebcb8b; }
details { margin-top: .6rem; border-left: 3px solid #bf616a;
          padding-left: .6rem; }
summary { cursor: pointer; color: #bf616a; }
.note { color: #7b8494; margin: .3rem 0; }
</style>
</head>
<body>
<h1>{{name}}</h1>
<p class="outcome">{{outcome}}</p>
<table class="manifest">{{manifest_rows}}</table>

<h2>video</h2>
{{film}}
<p class="note">{{video_note}}</p>

<h2>time line</h2>
<svg class="ruler" viewBox="0 0 {{width}} 22" preserveAspectRatio="none">
{{ruler}}</svg>
<ol>{{timeline}}</ol>

<h2>change and watches</h2>
{{strips}}

<h2>shots</h2>
<ul>{{shots}}</ul>

<h2>clips</h2>
<ul>{{clips}}</ul>

{{tails}}
<script>
(function () {
  var film = document.getElementById('film');
  // The rows are harness seconds; a strided film compressed that clock.
  var stride = {{film_stride}};
  function seek(at) {
    if (!film) return;
    film.currentTime = at / stride;
    var playing = film.play();
    if (playing && playing.catch) playing.catch(function () { });
  }
  document.querySelectorAll('[data-at]').forEach(function (row) {
    row.addEventListener('click', function () {
      seek(parseFloat(row.getAttribute('data-at')));
    });
  });
}());
</script>
</body>
</html>
)PAGE" };

  inline constexpr std::string_view FILM{
    R"FILM(<video id="film" src="{{video}}" controls)FILM"
    R"FILM( preload="metadata"></video>)FILM" };

  inline constexpr std::string_view MANIFEST_ROW{
    "<tr><td>{{field}}</td><td>{{value}}</td></tr>" };

  inline constexpr std::string_view VERDICT_ROW{
    R"ROW(<li class="{{outcome}}" data-at="{{at}}" title="seek the video">)ROW"
    R"ROW(<span class="frame">{{frame}}</span>)ROW"
    R"ROW(<span class="at">{{seconds}} s</span>)ROW"
    R"ROW(<span class="name">{{name}}</span> )ROW"
    R"ROW(<span class="word">{{outcome}}</span> {{text}}</li>)ROW" };

  inline constexpr std::string_view MARK_ROW{
    R"ROW(<li class="mark" data-at="{{at}}" title="seek the video">)ROW"
    R"ROW(<span class="frame">{{frame}}</span>)ROW"
    R"ROW(<span class="at">{{seconds}} s</span>mark {{text}}</li>)ROW" };

  inline constexpr std::string_view GROUP_ROW{
    R"ROW(<li class="mark group" data-at="{{at}}" title="seek the video">)ROW"
    R"ROW(<span class="frame">{{frame}}</span>)ROW"
    R"ROW(<span class="at">{{seconds}} s</span>)ROW"
    R"ROW(<span class="name">{{group}}</span> x{{count}}, )ROW"
    R"ROW(frames {{first}} to {{last}}</li>)ROW" };

  inline constexpr std::string_view RESTORE_ROW{
    R"ROW(<li class="rewind" data-at="{{at}}" title="seek the video">)ROW"
    R"ROW(<span class="frame">{{frame}}</span>)ROW"
    R"ROW(<span class="at">{{seconds}} s</span>rewound to frame {{to}}, )ROW"
    R"ROW(<span class="name">{{name}}</span></li>)ROW" };

  inline constexpr std::string_view REWIND_GROUP_ROW{
    R"ROW(<li class="rewind group" data-at="{{at}}" title="seek the video">)ROW"
    R"ROW(<span class="frame">{{frame}}</span>)ROW"
    R"ROW(<span class="at">{{seconds}} s</span>)ROW"
    R"ROW(<span class="name">rewound</span> x{{count}}, )ROW"
    R"ROW(frames {{first}} to {{last}}</li>)ROW" };

  inline constexpr std::string_view RESET_ROW{
    R"ROW(<li class="reset" data-at="{{at}}" title="seek the video">)ROW"
    R"ROW(<span class="frame">{{frame}}</span>)ROW"
    R"ROW(<span class="at">{{seconds}} s</span>)ROW"
    R"ROW(<span class="name">reset</span> to power on</li>)ROW" };

  inline constexpr std::string_view RESET_GROUP_ROW{
    R"ROW(<li class="reset group" data-at="{{at}}" title="seek the video">)ROW"
    R"ROW(<span class="frame">{{frame}}</span>)ROW"
    R"ROW(<span class="at">{{seconds}} s</span>)ROW"
    R"ROW(<span class="name">reset</span> x{{count}}, )ROW"
    R"ROW(frames {{first}} to {{last}}</li>)ROW" };

  inline constexpr std::string_view TICK{
    R"TICK(<line x1="{{x}}" y1="{{y}}" x2="{{x}}" y2="22")TICK"
    R"TICK( stroke="{{colour}}"><title>{{text}}</title></line>)TICK" };

  inline constexpr std::string_view STRIP{
    R"STRIP(<figcaption>{{name}} {{range}}</figcaption>
<svg class="plot" viewBox="0 0 {{width}} {{height}}")STRIP"
    R"STRIP( preserveAspectRatio="none">
<polyline points="{{points}}"/></svg>)STRIP" };

  inline constexpr std::string_view ARTIFACT_ROW{
    R"ROW(<li{{seek}}><span class="frame">{{frame}}</span>)ROW"
    R"ROW(<a href="{{href}}">{{name}}</a></li>)ROW" };

  inline constexpr std::string_view TAIL{
    R"TAIL(<details open><summary>{{title}}</summary>
<table class="tail">{{rows}}</table></details>)TAIL" };

  inline constexpr std::string_view TAIL_ROW{
    R"ROW(<tr><td class="frame">{{frame}}</td>)ROW"
    R"ROW(<td class="kind">{{kind}}</td><td>{{details}}</td></tr>)ROW" };
}
