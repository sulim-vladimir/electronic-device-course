// Creates submissions/step-NN.md (in Ukrainian) for every step in index.html.
// Existing files are never overwritten (they hold the student's answers).
//
//   node tools/make-submissions.js
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
  const deliver = (DELIVER[s.id] || []).map((d, k) => `### ${k + 1}. ${md(d).replace(/NN/g, n)}\n\n_(твоя відповідь)_\n`).join("\n");
  const checks = s.done.map((d) => `- [ ] ${md(d)}`).join("\n");
  const text = `# Крок ${n}: ${s.title}

**Мета:** ${md(s.goal)}

<!--
Наставник читатиме цей файл у твоєму pull request.
Заміни кожне «(твоя відповідь)» своїми словами. Повними реченнями, коротко — нормально.
Зображення: клади їх у submissions/img/ і показуй так:
![3D-перегляд](img/step-${n}-3d.png)
-->

## Що здати

${deliver}
## Прийнято, коли

Познач, що вже виконано (постав x у дужках: [x]).

${checks}

## Питання до наставника

_(немає)_
`;
  fs.writeFileSync(file, text);
  made++;
});
console.log(`created ${made} file(s) in ${dir}`);
