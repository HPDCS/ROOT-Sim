# Build the doxygen documentation
cd docs
make
cd ..

# Run the doc coverage pipeline
echo "-----------------------------------"
echo "   Documentation Coverage Report   "
echo "-----------------------------------"
REPORT=$(python3 -m coverxygen --xml-dir docs/xml/ --src-dir . --format summary --output - | tee doc-coverage.resume)
echo "-----------------------------------"
covpercent=$(cat doc-coverage.resume | grep Total | sed 's/.*://' | sed 's/%.*//' | xargs)
python3 -m coverxygen --xml-dir docs/xml/ --src-dir . --output doc-coverage.info
genhtml --no-function-coverage --no-branch-coverage doc-coverage.info -o docs/coverage/

# Post process and store the results
acceptable=$(echo $covpercent'>'50 | bc -l) # 60% is considered an acceptable threshold for now
color="red"
icon=":exclamation:"
if [ "$acceptable" -eq "1" ] ; then
	color="green"
	icon=":+1"
fi

# Setup the json file to provide data to shield.io
cat <<EOT > shields.json
{
  "schemaVersion": 1,
  "label": "doc coverage",
  "message": "$covpercent%",
  "color": "$color"
}
EOT

IFS='' read -r -d '' COMMENT <<EOT
Documentation coverage is **$covpercent%** $icon
[More detailed information](https://hpdcs.github.io/ROOT-Sim/docs/coverage/$TRAVIS_PULL_REQUEST/) is available.

\`\`\`
$REPORT
\`\`\`
EOT

# Escape json stuff
COMMENT=$(jq -aRs . <<< "$COMMENT")

echo "$COMMENT"

# Clone the github-pages website and setup paths
git clone --depth=1 -b gh-pages --single-branch https://${DOCS_SECRET}@github.com/HPDCS/ROOT-Sim.git
mkdir -p ROOT-Sim/docs/coverage/

# If running in a pull request, we copy coverage info in a dedicated path and add
# the PR information in the list of PR coverage. Also, in case of a PR, we print
# the results of the doc coverage analysis as a comment in the PR.
# Otherwise we copy it to a branch dedicated page, but we don't commit. This will
# be deployed by .travis.yml only for develop and master.
if [ "$TRAVIS_PULL_REQUEST" != "false" ] ; then
	# Copy the generated documentation coverage report in the website
	rm -rf ROOT-Sim/docs/coverage/$TRAVIS_PULL_REQUEST  # You can modify a PR with new commits
	mv docs/coverage ROOT-Sim/docs/coverage/$TRAVIS_PULL_REQUEST
	mv shields.json ROOT-Sim/docs/coverage/$TRAVIS_PULL_REQUEST.json

	# Add entry in the website log
	stamp=$(date +"%D %T")
	line="- [PR #$TRAVIS_PULL_REQUEST]({{ site.url }}/docs/coverage/$TRAVIS_PULL_REQUEST) ($stamp) ![coverage](https://img.shields.io/endpoint?url={{ site.url }}/docs/coverage/$TRAVIS_PULL_REQUEST.json)"
	sed -ie "/^Pull Requests:/a $line" ROOT-Sim/docs/coverage/index.md

	# Push back coverage information
	git config --global user.email "deploy@travis-ci.org"
	git config --global user.name "Travis CI"

	cd ROOT-Sim
	git add docs/coverage/$TRAVIS_PULL_REQUEST
	git add docs/coverage/$TRAVIS_PULL_REQUEST.json
	git add -u
	git commit --message "Travis doc build: $TRAVIS_BUILD_NUMBER"
	git push --quiet

	# Comment on GH
	curl -H "Authorization: token $DOCS_SECRET" -X POST -d "{\"body\": $COMMENT}" "https://api.github.com/repos/${TRAVIS_REPO_SLUG}/issues/${TRAVIS_PULL_REQUEST}/comments"

	cd ..
else
	# Copy the documentation related to the current branch (this is triggered only for master
	# and develop by the .travis.yml deploy section)
	rm -rf ROOT-Sim/docs/$TRAVIS_BRANCH
	mv docs/html ROOT-Sim/docs/$TRAVIS_BRANCH
	mv docs/coverage ROOT-Sim/docs/coverage/$TRAVIS_BRANCH
	mv shields.json ROOT-Sim/docs/coverage/$TRAVIS_BRANCH.json

	# Copy the current version readme file, which is used in the website About section
	cp README.md ROOT-Sim

	# We use also some content from the Wiki in the website, so clone it
	# and copy what is needed.
	git clone --depth=1 https://github.com/HPDCS/ROOT-Sim.wiki.git
	cp ROOT-Sim.wiki/chapters/part2/installation.md ROOT-Sim
	cp ROOT-Sim.wiki/chapters/part2/libraries.md ROOT-Sim
	cp ROOT-Sim.wiki/chapters/part2/sample-model.md ROOT-Sim
	cp ROOT-Sim.wiki/chapters/part2/usage.md ROOT-Sim
fi

