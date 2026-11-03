# Meteo Station Course

A step-by-step course that takes the weather station from the v2 schematic to assembled boards from JLCPCB.

- **Open the course:** double-click [`index.html`](index.html) (works offline), or use the online copy: https://claude.ai/artifact/U6qwXX3tTCxCMi7kED1e23
- **Your answers:** one file per step in [`submissions/`](submissions/), already prepared for you.
- **Practice board:** your practice KiCad project goes in `practice/` (the practice board step).

## How a step works

1. Start from an up-to-date `main` and make a branch for the step:
   ```
   git switch main
   git pull
   git switch -c step-04
   ```
2. Do the tasks from the course page.
3. Fill in `course/submissions/step-04.md`. Screenshots go in `course/submissions/img/`.
4. Commit and push:
   ```
   git status
   git add -A
   git commit -m "Step 04: How deep sleep works"
   git push -u origin step-04
   ```
5. On GitHub, open a **pull request** from `step-04` into `main`, titled like the commit.
6. Your mentor reviews it:
   - **Approved and merged:** the step is accepted. Run `git switch main` and `git pull`, then start the next step.
   - **Changes requested:** read the comments, fix, commit and push again on the same branch. The pull request updates by itself.

**One step at a time.** KiCad files are hard to merge, so only start the next step after the last one is merged.

## For the mentor: how to review a step

1. On GitHub, open **Pull requests** and pick "Step NN: ...".
2. **Files changed** tab:
   - Read `course/submissions/step-NN.md`. The "Accepted when" list at the bottom is what to check.
   - Open screenshots and the DRC report (`step-NN-drc.rpt`) if the step has them.
   - KiCad files show as long text diffs. You don't need to read them line by line. Check the screenshots and DRC report, or open the branch in KiCad yourself:
     ```
     git fetch
     git switch step-NN
     ```
3. Click **+** on any line to leave a comment there.
4. Finish with **Review changes**:
   - **Approve**, then **Merge pull request** and **Delete branch**: the step is accepted.
   - **Request changes** with a short list of what to fix: the step goes back for more work.
5. Steps marked **Mentor check** in the course (product definition, tools and budget, charging, enclosure, thermometer, final review and order, small series) need a real decision from you. Discuss in the pull request before approving.

## Maintaining the course

- The course is one file: `index.html`. Step references in the text are written as `{{stepId}}`, so step numbers update by themselves when steps are added.
- After adding steps, run `node course/tools/make-submissions.js`. It creates submission files for new steps and never overwrites existing ones. Steps that move to a new number keep their old file, so rename it if needed.
