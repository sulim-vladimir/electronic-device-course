// Creates course/submissions/step-NN.md for every step in course/index.html.
// Existing files are never overwritten (they hold the student's answers).
//
//   node course/tools/make-submissions.js
//
// Run it again after adding steps to the course.

const fs = require("fs");
const path = require("path");

const root = path.join(__dirname, "..");
const html = fs.readFileSync(path.join(root, "index.html"), "utf8");
const js = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const grab = (name) => eval("(" + js.match(new RegExp("const " + name + " = ([\\s\\S]*?\\n[\\]}]);"))[1] + ")");

const STEPS = grab("STEPS");
const DELIVER = grab("DELIVER");
const pad = (i) => String(i).padStart(2, "0");
const idx = (id) => STEPS.findIndex((s) => s.id === id);
const ref = (t) => t.replace(/\{\{(\w+)\}\}/g, (m, id) => (idx(id) < 0 ? m : String(idx(id))));
const md = (t) => ref(t)
  .replace(/<code>(.*?)<\/code>/g, "`$1`")
  .replace(/<b>(.*?)<\/b>/g, "**$1**")
  .replace(/<[^>]+>/g, "")
  .replace(/&amp;/g, "&").replace(/&lt;/g, "<").replace(/&gt;/g, ">");

const dir = path.join(root, "submissions");
fs.mkdirSync(path.join(dir, "img"), { recursive: true });

let made = 0;
STEPS.forEach((s, i) => {
  const n = pad(i);
  const file = path.join(dir, `step-${n}.md`);
  if (fs.existsSync(file)) return;
  const deliver = (DELIVER[s.id] || []).map((d, k) => `### ${k + 1}. ${md(d).replace(/NN/g, n)}\n\n_(your answer)_\n`).join("\n");
  const checks = s.done.map((d) => `- [ ] ${md(d)}`).join("\n");
  const text = `# Step ${n}: ${s.title}

**Goal:** ${md(s.goal)}

<!--
Your mentor reads this file in your pull request.
Replace every "(your answer)" with your own words. Full sentences, short is fine.
Pictures: put them in course/submissions/img/ and show them like this:
![3D view](img/step-${n}-3d.png)
-->

## Deliverables

${deliver}
## Accepted when

Tick what is true (put an x between the brackets: [x]).

${checks}

## Questions for my mentor

_(none)_
`;
  fs.writeFileSync(file, text);
  made++;
});
console.log(`created ${made} file(s) in ${dir}`);
